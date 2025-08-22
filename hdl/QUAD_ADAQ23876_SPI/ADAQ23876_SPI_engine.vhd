--
-- dual SPI engine for communication with 4 units of ADI ADC ADAQ23876
--


library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;
-- library UNISIM;
-- use UNISIM.VComponents.all;

entity ADAQ23876_SPI_engine is
  port
    (
    reset             :  in std_logic;
    clk               :  in std_logic;
    -- conv clk is external
    convclk           :  in std_logic;
    ADC_SCLK_div      :  in std_logic_vector( 3 downto 0);
    ADC_Tfirstclk     :  in std_logic_vector( 4 downto 0);
    -- lines to AXI interface
    sample_B_A        : out std_logic_vector(31 downto 0);
    sample_D_C        : out std_logic_vector(31 downto 0);
    sample_valid      : out std_logic;
    -- lines to SPI
    SCLK              : out std_logic;
    DA                :  in std_logic_vector(3 downto 0);
    DB                :  in std_logic_vector(3 downto 0)
    );
end ADAQ23876_SPI_engine;

architecture Behavioral of ADAQ23876_SPI_engine is

  type state is (IDLE, ACQUIRE, READBACK, SAMPLEOUT); 
  signal sm_state       : state;
  signal valid_r        : std_logic;
  type accum4 is array(0 to 3) of std_logic_vector(15 downto 0);
  signal accum          : accum4;
  signal cntr           : integer range 0 to 31;
  signal old_convclk    : std_logic;
  signal SCLK_r         : std_logic;
  signal sample_B_A_r   : std_logic_vector(31 downto 0);
  signal sample_D_C_r   : std_logic_vector(31 downto 0);
  signal SPI_CE         : std_logic;
  signal clk_cntr       : std_logic_vector(3 downto 0);
   
begin
  sample_valid      <= valid_r;
  SCLK              <= SCLK_r;
  sample_B_A        <= sample_B_A_r;
  sample_D_C        <= sample_D_C_r;

  -- clock divider for SPI ext clock ------------------------
  clk_div_cntr: process(clk, reset)
  begin
  if(rising_edge(clk)) then
    if(reset='1') then
      clk_cntr <= ADC_SCLK_div;
      SPI_CE   <= '0';
    else
      if(clk_cntr = "0000") then
        clk_cntr <= ADC_SCLK_div;
        SPI_CE   <= '1';
      else
        clk_cntr <= std_logic_vector(unsigned(clk_cntr)-1);
        SPI_CE   <= '0';
      end if;
    end if; -- if not reset
  end if;  -- if clock edge
  end process clk_div_cntr;

  -- main state machine -------------------------------------
  main_loop: process(clk, reset)
  begin
  if(rising_edge(clk)) then
    if(reset='1') then
      valid_r      <= '1';    -- valid is 0 only when reading new samples
      accumreset1: for i in 0 to 3 loop
        accum(i)   <= (others=>'0');
      end loop accumreset1;
      sample_B_A_r <= (others=>'0');
      sample_D_C_r <= (others=>'0');
      cntr         <= 0;
      old_convclk  <= '0';
      SCLK_r       <= '0';
      sm_state     <= IDLE;
    else
      old_convclk <= convclk;

      case(sm_state) is
        ---------------------------
        when IDLE =>
          accumreset2: for i in 0 to 3 loop
            accum(i)     <= (others=>'0');
          end loop accumreset2;
          cntr           <= 0;
          SCLK_r         <= '0';

          if((convclk='1') and (old_convclk='0')) then
            valid_r      <= '0';
            sample_B_A_r <= (others=>'0');
            sample_D_C_r <= (others=>'0');
            sm_state     <= ACQUIRE;
          else
            valid_r      <= valid_r;
            sample_B_A_r <= sample_B_A_r;
            sample_D_C_r <= sample_D_C_r;
            sm_state     <= IDLE;
          end if;
  
        ---------------------------
        when ACQUIRE =>
          accumreset3: for i in 0 to 3 loop
            accum(i)     <= (others=>'0');
          end loop accumreset3;
          valid_r        <= '0';
          sample_B_A_r   <= (others=>'0');
          sample_D_C_r   <= (others=>'0');
          SCLK_r         <= '0';

          if(cntr = to_integer(unsigned(ADC_Tfirstclk))) then
            cntr         <= 0;
            sm_state     <= READBACK;
          else
            cntr         <= cntr+1;
            sm_state     <= ACQUIRE;
          end if;

        ---------------------------
        when READBACK =>
          valid_r        <= '0';
          sample_B_A_r   <= (others=>'0');
          sample_D_C_r   <= (others=>'0');

          if(SPI_CE='1') then
            -- if clock enable, shift in the 2 new bits
            shift2: for i in 0 to 3 loop
              accum(i)     <= accum(i)(13 downto 0) & DA(i) & DB(i);
            end loop shift2;
  
            if(cntr = 7) then
              cntr         <= 0;
              SCLK_r       <= '0';
              sm_state     <= SAMPLEOUT;
            else
              cntr         <= cntr+1;
              SCLK_r       <= not SCLK_r;
              sm_state     <= READBACK;
            end if;

          else
            -- if not clock enable just latch everything
            latch1: for i in 0 to 3 loop
              accum(i)     <= accum(i);
            end loop latch1;
            cntr           <= cntr;
            SCLK_r         <= SCLK_r;
            sm_state       <= READBACK;
          end if;  -- if clock enable

        ---------------------------
        when SAMPLEOUT =>
          sample_B_A_r   <= accum(1) & accum(0);
          sample_D_C_r   <= accum(3) & accum(2);
          valid_r        <= '1';
          accumreset5: for i in 0 to 3 loop
            accum(i)     <= (others=>'0');
          end loop accumreset5;
          cntr           <= 0;
          SCLK_r         <= '0';
          sm_state       <= IDLE;

        ---------------------------
        when others =>
          valid_r      <= '1';
          accumreset6: for i in 0 to 3 loop
            accum(i)   <= (others=>'0');
          end loop accumreset6;
          sample_B_A_r <= (others=>'0');
          sample_D_C_r <= (others=>'0');
          cntr         <= 0;
          old_convclk  <= '0';
          SCLK_r       <= '0';
          sm_state     <= IDLE;
      end case;
    end if; -- if not reset
  end if;  -- if clock edge
  end process main_loop;

end Behavioral;
