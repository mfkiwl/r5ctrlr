library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity QUAD_streamer is
  generic
    (
    C_M_AXIS_TDATA_WIDTH    : integer	:= 64
    );
  port
    (
    -- stream master
    M_AXIS_ACLK	     : in std_logic;
    M_AXIS_ARESETN	 : in std_logic;
    M_AXIS_TVALID	   : out std_logic;
    M_AXIS_TDATA	   : out std_logic_vector(C_M_AXIS_TDATA_WIDTH-1 downto 0);
    M_AXIS_TSTRB	   : out std_logic_vector((C_M_AXIS_TDATA_WIDTH/8)-1 downto 0);
    M_AXIS_TKEEP	   : out std_logic_vector((C_M_AXIS_TDATA_WIDTH/8)-1 downto 0);
    M_AXIS_TLAST 	   : out std_logic;
    M_AXIS_TREADY	   : in std_logic;

    -- from SPI engine
    sample_B_A        :  in std_logic_vector(31 downto 0);
    sample_D_C        :  in std_logic_vector(31 downto 0);
    sample_valid      :  in std_logic
    );
end QUAD_streamer;

architecture arch_imp of QUAD_streamer is

  type state is (IDLE, STREAMOUT);
  signal  sm_state : state;

  signal data  : std_logic_vector(C_M_AXIS_TDATA_WIDTH-1 downto 0);
  signal valid : std_logic;
  signal old_sample_valid : std_logic;

begin

M_AXIS_TSTRB  <= (others => '1');
M_AXIS_TKEEP  <= (others => '1');
M_AXIS_TLAST  <= '0';
M_AXIS_TDATA  <= data;
M_AXIS_TVALID <= valid;

state_machine : process(M_AXIS_ACLK)
  begin
    if (rising_edge (M_AXIS_ACLK)) then
      old_sample_valid <= sample_valid;

      if(M_AXIS_ARESETN = '0') then
        data  <= (others => '0');
        valid <= '0';
        sm_state <= IDLE;
      else
        case(sm_state) is

          when IDLE =>
            data <= sample_D_C & sample_B_A;
            if((sample_valid='1') and (old_sample_valid='0')) then
              valid <= '1';
              sm_state <= STREAMOUT;
            else
              valid <= '0';
              sm_state <= IDLE;
            end if;
          
          when STREAMOUT =>
            -- valid is '1' for sure
            if(M_AXIS_TREADY='1') then
              data <= sample_D_C & sample_B_A;
              valid <= '0';
              sm_state <= IDLE;
            else
              data <= data;
              valid <= '1';
              sm_state <= STREAMOUT;
            end if;
          
          when others =>
            data <= sample_D_C & sample_B_A;
            valid <= '0';
            sm_state <= IDLE;

        end case;
      end if;  -- if not reset
    end if;  -- if CLK edge
  end process state_machine;

end arch_imp;
