library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity dual_streamer is
  generic
    (
    C_S_AXIS_TDATA_WIDTH    : integer := 32
    );
  port
    (
    -- stream slave
    S_AXIS_ACLK       :  in std_logic;
    S_AXIS_ARESETN    :  in std_logic;
    S_AXIS_TVALID     :  in std_logic;
    S_AXIS_TDATA      :  in std_logic_vector(C_S_AXIS_TDATA_WIDTH-1 downto 0);
    S_AXIS_TSTRB      :  in std_logic_vector((C_S_AXIS_TDATA_WIDTH/8)-1 downto 0);
    S_AXIS_TKEEP      :  in std_logic_vector((C_S_AXIS_TDATA_WIDTH/8)-1 downto 0);
    S_AXIS_TLAST      :  in std_logic;
    S_AXIS_TREADY     : out std_logic;

    -- to CMD interpreter
    start_transaction : out std_logic;
    busy              :  in std_logic;
    DAC_RW            : out std_logic;
    DAC_addr_data     : out std_logic;
    DAC_LAST          : out std_logic;
    DAC_bytenum       : out std_logic_vector(1 downto 0);
    DAC_wr_word       : out std_logic_vector(23 downto 0)
    );
end dual_streamer;

architecture arch_imp of dual_streamer is

  type state is (IDLE, WAIT_BUSY1, SEND_DAC1, WAIT_BUSY2, SEND_DAC2, WAIT_BUSY3);
  signal  sm_state : state;

  signal start, addr_data, last, ready : std_logic;
  signal bytenum : std_logic_vector(1 downto 0);
  signal data : std_logic_vector(23 downto 0);
  signal samples : std_logic_vector(C_S_AXIS_TDATA_WIDTH-1 downto 0);

begin
  start_transaction <= start;
  DAC_RW            <= '0';
  DAC_addr_data     <= addr_data;
  DAC_LAST          <= last;
  DAC_bytenum       <= bytenum;
  DAC_wr_word       <= data;
  S_AXIS_TREADY     <= ready;

  state_machine : process(S_AXIS_ACLK)
  begin
    if (rising_edge (S_AXIS_ACLK)) then
      if(S_AXIS_ARESETN = '0') then
        start     <= '0';
        addr_data <= '0';
        last      <= '0';
        bytenum   <= "00";
        data      <= (others => '0');
        samples   <= (others => '0');
        ready     <= '0';
        sm_state  <= IDLE;
      else
        case(sm_state) is

          ----------------------------
          when IDLE =>
            if((S_AXIS_TVALID='1')and(ready='1')) then
              -- we have a new sample: send address first
              addr_data <= '1';
              last      <= '0';
              bytenum   <= "01";
              data      <= x"00004B";
              start     <= '1';
              ready     <= '0';
              samples   <= S_AXIS_TDATA;
              sm_state  <= WAIT_BUSY1;
            else
              -- we might have last data phase still pending -> busy high
              addr_data <= addr_data;
              last      <= last;
              bytenum   <= bytenum;
              data      <= data;
              start     <= '0';
              ready     <= not busy;
              samples   <= (others => '0');
              sm_state  <= IDLE;
            end if;

          ----------------------------
          when WAIT_BUSY1 =>
            addr_data <= addr_data;
            last      <= last;
            bytenum   <= bytenum;
            data      <= data;
            start     <= '0';
            ready     <= '0';
            samples   <= samples;
            if(busy='0') then
              -- wait for address phase to start
              sm_state  <= WAIT_BUSY1;
            else
              -- address phase started; proceed to first DAC sample
              sm_state  <= SEND_DAC1;
            end if;

          ----------------------------
          when SEND_DAC1 =>
            if(busy='1') then
              -- wait for address phase to end
              addr_data <= addr_data;
              last      <= last;
              bytenum   <= bytenum;
              data      <= data;
              start     <= '0';
              ready     <= '0';
              samples   <= samples;
              sm_state  <= SEND_DAC1;
            else
              -- address phase is over; send first DAC sample
              addr_data <= '0';
              last      <= '0';
              bytenum   <= "11";
              data      <= "00000000" & samples(7 downto 0) & samples(15 downto 8);
              start     <= '1';
              ready     <= '0';
              samples   <= samples;
              sm_state  <= WAIT_BUSY2;
            end if;

          ----------------------------
          when WAIT_BUSY2 =>
            addr_data <= addr_data;
            last      <= last;
            bytenum   <= bytenum;
            data      <= data;
            start     <= '0';
            ready     <= '0';
            samples   <= samples;
            if(busy='0') then
              -- wait for first data phase to start
              sm_state  <= WAIT_BUSY2;
            else
              -- data phase started; proceed to second DAC sample
              sm_state  <= SEND_DAC2;
            end if;

          ----------------------------
          when SEND_DAC2 =>
            if(busy='1') then
              -- wait for first DAC sample to be transmitted
              addr_data <= addr_data;
              last      <= last;
              bytenum   <= bytenum;
              data      <= data;
              start     <= '0';
              ready     <= '0';
              samples   <= samples;
              sm_state  <= SEND_DAC2;
            else
              -- first DAC sample is in; now send second DAC sample
              addr_data <= '0';
              last      <= '1';
              bytenum   <= "11";
              data      <= "00000000" & samples(23 downto 16) & samples(31 downto 24);
              start     <= '1';
              ready     <= '0';
              samples   <= samples;
              sm_state  <= WAIT_BUSY3;
            end if;

          ----------------------------
          when WAIT_BUSY3 =>
            addr_data <= addr_data;
            last      <= last;
            bytenum   <= bytenum;
            data      <= data;
            start     <= '0';
            ready     <= '0';
            samples   <= samples;
            if(busy='0') then
              -- wait for second data phase to start
              sm_state  <= WAIT_BUSY3;
            else
              -- data phase started; revert to IDLE (it will take care of the ongoing BUSY)
              sm_state  <= IDLE;
            end if;

          when others =>
            start     <= '0';
            addr_data <= '0';
            last      <= '0';
            bytenum   <= "00";
            data      <= (others => '0');
            samples   <= (others => '0');
            ready     <= '0';
            sm_state  <= IDLE;

        end case;
      end if;  -- if not reset
    end if;  -- if CLK edge
  end process state_machine;

end arch_imp;
