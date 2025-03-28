--
-- quad SPI engine for communication with ADI DAC AD3552
--
-- latest rev: mar 19 2025
--

library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;
-- library UNISIM;
-- use UNISIM.VComponents.all;
entity AD3552_CMD_interpreter is
  port
    (
    reset             :  in std_logic;
    clk               :  in std_logic;
    -- lines to the register bank
    start_transaction :  in std_logic;
    busy              : out std_logic;
    DAC_SPI_CLK_div   :  in std_logic_vector(1 downto 0);
    DAC_RW            :  in std_logic;
    DAC_addr_data     :  in std_logic;
    DAC_LAST          :  in std_logic;
    DAC_bytenum       :  in std_logic_vector(1 downto 0);
    DAC_wr_word       :  in std_logic_vector(23 downto 0);
    DAC_rd_word       : out std_logic_vector(23 downto 0);
    errcode           : out std_logic_vector(3 downto 0);
    -- lines to the SPI engine
    SPI_reset         : out std_logic;
    SPI_CE            : out std_logic;
    SPI_wbyte         : out std_logic_vector(7 downto 0);
    SPI_rbyte         :  in std_logic_vector(7 downto 0);
    SPI_RW            : out std_logic;
    SPI_start         : out std_logic;
    SPI_busy          :  in std_logic;
    -- output lines
    SPI_CSn           : out std_logic
    );

end AD3552_CMD_interpreter;

architecture Behavioral of AD3552_CMD_interpreter is

  type state is ( IDLE, WAIT_CS, SHIFT_ADDR, SHIFT_BYTE);
  constant NOERR           : std_logic_vector(3 downto 0) := "0000";
  constant ERR_RW_MISMATCH : std_logic_vector(3 downto 0) := "0001";
  constant ERR_OVERRUN     : std_logic_vector(3 downto 0) := "0010";
  constant ERR_ZERO_BYTES  : std_logic_vector(3 downto 0) := "0011";
  constant ERR_SPI_BUSY    : std_logic_vector(3 downto 0) := "0100";
  signal sm_state  : state;
  signal old_start : std_logic;
  signal old_busy : std_logic;
  signal SPI_wbyte_r : std_logic_vector(7 downto 0);
  signal SPI_RW_r : std_logic;
  signal errcode_r : std_logic_vector(3 downto 0);
  signal SPI_CEr : std_logic;
  signal clk_cntr  : std_logic_vector(2 downto 0);
  -- "save" signals for multibyte transactions
  signal save_RW, save_LAST  : std_logic;
  signal save_wr_word  : std_logic_vector(15 downto 0);
  signal save_rd_word, DAC_rd_word_latch : std_logic_vector(23 downto 0);
  signal bytecnt   : unsigned(1 downto 0);
  signal save_BYTENUM : std_logic_vector(1 downto 0);
  -- "static" signals to keep things at the right place between transactions
  signal RW_static : std_logic;
  signal CSn_static : std_logic;

begin

  SPI_CSn     <= CSn_static;
  SPI_wbyte   <= SPI_wbyte_r;
  SPI_RW      <= SPI_RW_r;
  DAC_rd_word <= DAC_rd_word_latch;
  errcode     <= errcode_r;

  -- clock divider for SPI ext clock ------------------------
  SPI_CE <= SPI_CEr;

  with DAC_SPI_CLK_div select
    SPI_CEr <= '1'         when "00",
               clk_cntr(0) when "01",
               clk_cntr(1) when "10",
               clk_cntr(2) when "11",
               '1'         when others;

  clk_div_cntr: process(clk, reset)
  begin
  if(rising_edge(clk)) then
    if(reset='1') then
      clk_cntr <= (others => '0');
    else
      clk_cntr <= std_logic_vector(unsigned(clk_cntr)+1);
    end if; -- if not reset
  end if;  -- if clock edge
  end process clk_div_cntr;

  -- main state machine -------------------------------------
  main_loop: process(clk, reset)
  begin
  if(rising_edge(clk)) then
    if(reset='1') then
      save_rd_word      <= (others => '0');
      save_wr_word      <= (others => '0');
      DAC_rd_word_latch <= (others => '0');
      SPI_wbyte_r       <= (others => '0');
      save_RW           <= '1';
      save_LAST         <= '0';
      save_BYTENUM      <= "00";
      bytecnt           <= to_unsigned(0,bytecnt'length);
      errcode_r         <= NOERR;
      old_start         <= '0';
      old_busy          <= '0';
      busy              <= '0';
      SPI_reset         <= '1';
      SPI_start         <= '0';
      CSn_static        <= '1';
      SPI_RW_r          <= '1';
      RW_static         <= '1';
      sm_state          <= IDLE;
    else
      SPI_reset    <= '0';
      old_start    <= start_transaction;
      old_busy     <= SPI_busy;
      RW_static    <= RW_static;

      case(sm_state) is
        ---------------------------
        when IDLE =>
          DAC_rd_word_latch <= DAC_rd_word_latch;
          save_rd_word      <= (others => '0');

          if((start_transaction='1') and (old_start='0') ) then
            busy <= '1';
            if(DAC_bytenum="00") then
              -- error: transaction of 0 bytes ends right away
              errcode_r      <= ERR_ZERO_BYTES;
              save_wr_word   <= (others => '0');
              SPI_wbyte_r    <= SPI_wbyte_r;
              SPI_RW_r       <= '1';
              CSn_static     <= '1';
              SPI_start      <= '0';
              sm_state       <= IDLE;
            elsif(SPI_busy='1') then
              -- error: SPI engine busy
              errcode_r      <= ERR_SPI_BUSY;
              save_wr_word   <= (others => '0');
              SPI_wbyte_r    <= SPI_wbyte_r;
              SPI_RW_r       <= '1';
              CSn_static     <= '1';
              SPI_start      <= '0';
              sm_state       <= IDLE;
            elsif(DAC_addr_data='1') then
              -- transmit address
              if(CSn_static='1') then
                -- if CS is high, start a new transaction pulling CSn low
                errcode_r    <= NOERR;
                CSn_static   <= '0';
                SPI_wbyte_r  <= DAC_wr_word(7 downto 0);
                RW_static    <= DAC_wr_word(7);
                save_wr_word <= (others => '0');
                SPI_RW_r     <= '0';
                SPI_start    <= '1';
                sm_state     <= SHIFT_ADDR;
              else
                -- if CS is low, last transaction is still pending; 
                -- report an error and abort
                save_wr_word <= (others => '0');
                errcode_r    <= ERR_OVERRUN;
                SPI_wbyte_r  <= SPI_wbyte_r;
                SPI_RW_r     <= SPI_RW_r;
                CSn_static   <= CSn_static;
                SPI_start    <= '0';
                sm_state     <= IDLE;
              end if;
            else
              -- transmit or receive data
              --
              -- first of all check that RW is coherent 
              -- with the RW bit in previous address transaction
              if(DAC_RW /= RW_static) then
                -- RW is not coherent: abort
                errcode_r      <= ERR_RW_MISMATCH;
                save_wr_word   <= (others => '0');
                SPI_wbyte_r    <= SPI_wbyte_r;
                SPI_RW_r       <= '1';
                CSn_static     <= '1';
                SPI_start      <= '0';
                sm_state       <= IDLE;
              else
                -- RW is coherent: proceed with transaction
                errcode_r      <= NOERR;
                CSn_static     <= '0';
                save_RW        <= DAC_RW;
                save_LAST      <= DAC_LAST;
                save_BYTENUM   <= DAC_bytenum;
                bytecnt        <= unsigned(DAC_bytenum)-1;
                save_wr_word   <= DAC_wr_word(23 downto 8);
                SPI_wbyte_r    <= DAC_wr_word(7 downto 0);
                SPI_RW_r       <= DAC_RW;
                SPI_start      <= '1';
                sm_state       <= SHIFT_BYTE;
              end if;
            end if;
          else
            save_wr_word     <= (others => '0');
            SPI_wbyte_r      <= SPI_wbyte_r;
            CSn_static       <= CSn_static;
            errcode_r        <= errcode_r;
            busy             <= '0';
            SPI_start        <= '0';
            SPI_RW_r         <= RW_static;
            sm_state         <= IDLE;
          end if;

        ---------------------------
        when SHIFT_ADDR =>
          DAC_rd_word_latch <= DAC_rd_word_latch;
          CSn_static        <= CSn_static;
          SPI_wbyte_r       <= SPI_wbyte_r;
          errcode_r         <= errcode_r;
          if((SPI_busy='0') and (old_busy='1')) then
            SPI_start   <= '0';
            busy        <= '0';
            SPI_RW_r    <= RW_static;
            sm_state    <= IDLE;
          else
            SPI_start   <= '1';
            busy        <= '1';
            SPI_RW_r    <= '0';
            sm_state    <= SHIFT_ADDR;
          end if;

        ---------------------------
        when SHIFT_BYTE =>
          save_RW      <= save_RW;
          save_LAST    <= save_LAST;
          save_BYTENUM <= save_BYTENUM;
          errcode_r    <= errcode_r;
          SPI_RW_r     <= SPI_RW_r;

          if((SPI_busy='0') and (old_busy='1')) then
            -- SPI engine done; send next byte
            if(bytecnt = 0) then
              if(save_LAST = '1') then
                CSn_static <= '1';
              else
                CSn_static <= '0';
              end if;
              save_rd_word <= save_rd_word;
              save_wr_word <= save_wr_word;
              bytecnt      <= bytecnt;
              SPI_wbyte_r  <= SPI_wbyte_r;
              SPI_start    <= '0';
              busy         <= '0';
              sm_state     <= IDLE;
              -- rearrange readback word to have the first read byte as LSByte
              case(save_BYTENUM) is
                when "01" =>
                  DAC_rd_word_latch <= "0000000000000000" & save_rd_word(23 downto 16);
                when "10" =>
                  DAC_rd_word_latch <= "00000000" & save_rd_word(23 downto 8);
                when "11" =>
                  DAC_rd_word_latch <= save_rd_word;
                when others =>
                  DAC_rd_word_latch <= save_rd_word;
              end case;
            else
              busy         <= '1';
              DAC_rd_word_latch <= DAC_rd_word_latch;
              CSn_static   <= '0';
              bytecnt      <= bytecnt -1;
              save_wr_word <= "00000000" & save_wr_word(15 downto 8);
              SPI_wbyte_r  <= save_wr_word(7 downto 0);
              save_rd_word <= SPI_rbyte & save_rd_word(15 downto 8);
              SPI_start    <= '0';
              sm_state     <= SHIFT_BYTE;
            end if;
          else
            -- SPI engine not done yet
            SPI_start    <= '1';
            busy         <= '1';
            DAC_rd_word_latch <= DAC_rd_word_latch;
            CSn_static        <= CSn_static;
            SPI_wbyte_r       <= SPI_wbyte_r;
            save_rd_word <= save_rd_word;
            save_wr_word <= save_wr_word;
            bytecnt      <= bytecnt;
            sm_state     <= SHIFT_BYTE;
          end if;

        ---------------------------
        when others =>
          save_rd_word      <= (others => '0');
          save_wr_word      <= (others => '0');
          DAC_rd_word_latch <= DAC_rd_word_latch;
          SPI_wbyte_r       <= SPI_wbyte_r;
          busy              <= '0';
          sm_state          <= IDLE;
          errcode_r         <= errcode_r;
          SPI_start         <= '0';
          CSn_static        <= '1';
          SPI_RW_r          <= '1';

      end case;
    end if; -- if not reset
  end if;  -- if clock edge
  end process main_loop;


end Behavioral;
