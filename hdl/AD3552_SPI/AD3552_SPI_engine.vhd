--
-- quad SPI engine for communication with ADI DAC AD3552
--


library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
-- use IEEE.NUMERIC_STD.ALL;
-- library UNISIM;
-- use UNISIM.VComponents.all;

entity AD3552_SPI_engine is
  port
    (
    reset             :  in std_logic;
    clk               :  in std_logic;
    ce                :  in std_logic;
    -- lines to command interpreter
    start_transaction :  in std_logic;
    busy              : out std_logic;
    wbyte             :  in std_logic_vector(7 downto 0);
    rbyte             : out std_logic_vector(7 downto 0);
    RW                :  in std_logic;
    nextRW            :  in std_logic;
    -- lines to SPI
    XLATOR_DIR        : out std_logic; -- '1' means input to FPGA
    SCLK              : out std_logic;
    SDIO_i            :  in std_logic_vector(3 downto 0);
    SDIO_o            : out std_logic_vector(3 downto 0);
    SDIO_t            : out std_logic_vector(3 downto 0)
    );
end AD3552_SPI_engine;

architecture Behavioral of AD3552_SPI_engine is

  type state is ( IDLE, SHIFTnibble1, SHIFTnibble2); 
  signal sm_state       : state;
  signal nib_shiftin, nib_shiftout : std_logic_vector(3 downto 0);
  signal rbyte_reg      : std_logic_vector(7 downto 0);
  signal old_start      : std_logic;
  signal SCLK_reg       : std_logic;
  signal SDIO_o_r       : std_logic_vector(3 downto 0);
  signal SDIO_t_r       : std_logic_vector(3 downto 0);
  signal XLATOR_DIR_r   : std_logic;
  signal busy_r         : std_logic;
  signal RW_save, nextRW_save : std_logic;
   
begin
  rbyte      <= rbyte_reg;
  SCLK       <= SCLK_reg;
  SDIO_o     <= SDIO_o_r;
  SDIO_t     <= SDIO_t_r;
  XLATOR_DIR <= XLATOR_DIR_r;
  busy       <= busy_r;

  main_loop: process(clk, reset)
  begin
  if(rising_edge(clk)) then
    if(reset='1') then
      old_start    <= '0';
      RW_save      <= '1';
      nextRW_save  <= '1';
      SDIO_t_r     <= (others => '1');
      XLATOR_DIR_r <= '1';
      busy_r       <= '0';
      nib_shiftout <= (others => '0');
      SDIO_o_r     <= (others => '0');
      nib_shiftin  <= (others => '0');
      rbyte_reg    <= rbyte_reg;
      SCLK_reg     <= '0';
      sm_state     <= IDLE;
    else
      -- the test on CE = clock enable, to allow for the use of a divided clock
      -- is not done here globally, which would be simpler and would run the whole 
      -- state machine at the divided clock rate;
      -- instead, it is done case by case, to allow for fast response when needed;
      -- for example, in the release of the outputs (+translator direction switch)
      -- as soon as an incoming read transaction is detected

      if(ce='1') then
        old_start <= start_transaction;
      else
        old_start <= old_start;
      end if;

      case(sm_state) is
        ---------------------------
        when IDLE =>
          if((start_transaction='1') and (old_start='0') and (ce='1')) then
            busy_r       <= '1';
            RW_save      <= RW;
            nextRW_save  <= nextRW;
            SDIO_t_r     <= (others => RW);
            XLATOR_DIR_r <= RW;
            nib_shiftout <= wbyte(3 downto 0);
            SDIO_o_r     <= wbyte(7 downto 4);
            nib_shiftin  <= SDIO_i;
            rbyte_reg    <= rbyte_reg;
            SCLK_reg     <= '0';
            sm_state     <= SHIFTnibble1;
          else
            busy_r       <= '0';
            RW_save      <= RW_save;
            nextRW_save  <= nextRW_save;
            SDIO_t_r     <= (others => nextRW_save);
            XLATOR_DIR_r <= nextRW_save;
            nib_shiftout <= (others => '0');
            SDIO_o_r     <= (others => '0');
            nib_shiftin  <= (others => '0');
            rbyte_reg    <= rbyte_reg;
            SCLK_reg     <= '0';
            sm_state     <= IDLE;
          end if;
  
        ---------------------------
        when SHIFTnibble1 =>
          busy_r         <= '1';
          nib_shiftout   <= nib_shiftout;
          nib_shiftin    <= nib_shiftin;
          rbyte_reg      <= rbyte_reg;
          SDIO_t_r       <= SDIO_t_r;
          XLATOR_DIR_r   <= XLATOR_DIR_r;
          RW_save        <= RW_save;
          nextRW_save    <= nextRW_save;
          SDIO_t_r       <= SDIO_t_r;
          XLATOR_DIR_r   <= XLATOR_DIR_r;
          if(ce='1') then
            if(SCLK_reg='0') then
              SDIO_o_r     <= SDIO_o_r;
              SCLK_reg     <= '1';
              sm_state     <= SHIFTnibble1;
            else
              SDIO_o_r     <= nib_shiftout;
              SCLK_reg     <= '0';
              sm_state     <= SHIFTnibble2;
            end if;
          else
            -- if not CE
            SDIO_o_r     <= SDIO_o_r;
            SCLK_reg     <= SCLK_reg;
            sm_state     <= SHIFTnibble1;
          end if;

        ---------------------------
        when SHIFTnibble2 =>
          busy_r       <= '1';
          nib_shiftout <= nib_shiftout;
          SDIO_o_r     <= SDIO_o_r;
          nib_shiftin  <= nib_shiftin;
          rbyte_reg    <= nib_shiftin & SDIO_i;
          RW_save      <= RW_save;
          nextRW_save  <= nextRW_save;
          SDIO_t_r     <= SDIO_t_r;
          XLATOR_DIR_r <= XLATOR_DIR_r;
          if(ce='1') then
            if(SCLK_reg='0') then
              SCLK_reg     <= '1';
              sm_state     <= SHIFTnibble2;
            else
              SCLK_reg     <= '0';
              sm_state     <= IDLE;
            end if;
          else
            -- if not CE
            SCLK_reg     <= SCLK_reg;
            sm_state     <= SHIFTnibble2;
          end if;

        ---------------------------
        when others =>
          busy_r       <= '0';
          nib_shiftout <= (others => '0');
          SDIO_o_r     <= (others => '0');
          nib_shiftin  <= (others => '0');
          rbyte_reg    <= rbyte_reg;
          SCLK_reg     <= '0';
          RW_save      <= '1';
          nextRW_save  <= '1';
          SDIO_t_r     <= (others => '1');
          XLATOR_DIR_r <= '1';
          sm_state     <= IDLE;
      end case;
      --else
      --  -- if not clock enable, just latch everything
      --  old_start    <= old_start;
      --  RW_save      <= RW_save;
      --  nextRW_save  <= nextRW_save;
      --  SDIO_t_r     <= SDIO_t_r;
      --  XLATOR_DIR_r <= XLATOR_DIR_r;
      --  busy_r       <= busy_r;
      --  nib_shiftout <= nib_shiftout;
      --  SDIO_o_r     <= SDIO_o_r;
      --  nib_shiftin  <= nib_shiftin;
      --  rbyte_reg    <= rbyte_reg;
      --  SCLK_reg     <= SCLK_reg;
      --  sm_state     <= sm_state;        
      --end if; -- if clock enable
    end if; -- if not reset
  end if;  -- if clock edge
  end process main_loop;

end Behavioral;
