library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
library UNISIM;
use UNISIM.VComponents.all;


entity QUAD_ADAQ23876 is
	generic (
		-- Users to add parameters here

    -- stream master intf
		C_M_AXIS_TDATA_WIDTH    : integer	:= 64;

		-- User parameters ends
		-- Do not modify the parameters beyond this line

		-- Parameters of Axi Slave Bus Interface S00_AXI
		C_S00_AXI_DATA_WIDTH	: integer	:= 32;
		C_S00_AXI_ADDR_WIDTH	: integer	:= 5
	);
	port (
		-- Ports of Axi Slave Bus Interface S00_AXI
		s00_axi_aclk	: in std_logic;
		s00_axi_aresetn	: in std_logic;

  -- Users to add ports here
    conv_strobe   :  in std_logic;
    cnv_p         : out std_logic_vector(3 downto 0);
    cnv_n         : out std_logic_vector(3 downto 0);
    DA_p          :  in std_logic_vector(3 downto 0);
    DA_n          :  in std_logic_vector(3 downto 0);
    DB_p          :  in std_logic_vector(3 downto 0);
    DB_n          :  in std_logic_vector(3 downto 0);
    SCLK_p        : out std_logic_vector(3 downto 0);
    SCLK_n        : out std_logic_vector(3 downto 0);
  
    -- stream master
		M_AXIS_ACLK	     : in std_logic;
		M_AXIS_ARESETN	 : in std_logic;
		M_AXIS_TVALID	   : out std_logic;
		M_AXIS_TDATA	   : out std_logic_vector(C_M_AXIS_TDATA_WIDTH-1 downto 0);
		M_AXIS_TSTRB	   : out std_logic_vector((C_M_AXIS_TDATA_WIDTH/8)-1 downto 0);
		M_AXIS_TKEEP	   : out std_logic_vector((C_M_AXIS_TDATA_WIDTH/8)-1 downto 0);
		M_AXIS_TLAST 	   : out std_logic;
		M_AXIS_TREADY	   : in std_logic;


		-- User ports ends
		-- Do not modify the ports beyond this line


		-- Ports of Axi Slave Bus Interface S00_AXI
		s00_axi_awaddr	: in std_logic_vector(C_S00_AXI_ADDR_WIDTH-1 downto 0);
		s00_axi_awprot	: in std_logic_vector(2 downto 0);
		s00_axi_awvalid	: in std_logic;
		s00_axi_awready	: out std_logic;
		s00_axi_wdata	: in std_logic_vector(C_S00_AXI_DATA_WIDTH-1 downto 0);
		s00_axi_wstrb	: in std_logic_vector((C_S00_AXI_DATA_WIDTH/8)-1 downto 0);
		s00_axi_wvalid	: in std_logic;
		s00_axi_wready	: out std_logic;
		s00_axi_bresp	: out std_logic_vector(1 downto 0);
		s00_axi_bvalid	: out std_logic;
		s00_axi_bready	: in std_logic;
		s00_axi_araddr	: in std_logic_vector(C_S00_AXI_ADDR_WIDTH-1 downto 0);
		s00_axi_arprot	: in std_logic_vector(2 downto 0);
		s00_axi_arvalid	: in std_logic;
		s00_axi_arready	: out std_logic;
		s00_axi_rdata	: out std_logic_vector(C_S00_AXI_DATA_WIDTH-1 downto 0);
		s00_axi_rresp	: out std_logic_vector(1 downto 0);
		s00_axi_rvalid	: out std_logic;
		s00_axi_rready	: in std_logic
	);
end QUAD_ADAQ23876;

architecture arch_imp of QUAD_ADAQ23876 is

	-- component declaration
	component QUAD_ADAQ23876_slave_lite_v1_0_S00_AXI is
		generic (
		C_S_AXI_DATA_WIDTH	: integer	:= 32;
		C_S_AXI_ADDR_WIDTH	: integer	:= 5
		);
		port (
    ADC_SCLK_div      : out std_logic_vector( 3 downto 0);
    ADC_Tfirstclk     : out std_logic_vector( 4 downto 0);
    sample_B_A        :  in std_logic_vector(31 downto 0);
    sample_D_C        :  in std_logic_vector(31 downto 0);
    sample_valid      :  in std_logic;
    
    -- AXI-lite slave
    S_AXI_ACLK	: in std_logic;
		S_AXI_ARESETN	: in std_logic;
		S_AXI_AWADDR	: in std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0);
		S_AXI_AWPROT	: in std_logic_vector(2 downto 0);
		S_AXI_AWVALID	: in std_logic;
		S_AXI_AWREADY	: out std_logic;
		S_AXI_WDATA	: in std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
		S_AXI_WSTRB	: in std_logic_vector((C_S_AXI_DATA_WIDTH/8)-1 downto 0);
		S_AXI_WVALID	: in std_logic;
		S_AXI_WREADY	: out std_logic;
		S_AXI_BRESP	: out std_logic_vector(1 downto 0);
		S_AXI_BVALID	: out std_logic;
		S_AXI_BREADY	: in std_logic;
		S_AXI_ARADDR	: in std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0);
		S_AXI_ARPROT	: in std_logic_vector(2 downto 0);
		S_AXI_ARVALID	: in std_logic;
		S_AXI_ARREADY	: out std_logic;
		S_AXI_RDATA	: out std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
		S_AXI_RRESP	: out std_logic_vector(1 downto 0);
		S_AXI_RVALID	: out std_logic;
		S_AXI_RREADY	: in std_logic
		);
	end component QUAD_ADAQ23876_slave_lite_v1_0_S00_AXI;

component ADAQ23876_SPI_engine is
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
end component ADAQ23876_SPI_engine;


component QUAD_streamer is
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
end component QUAD_streamer;


signal sample_B_A    : std_logic_vector(31 downto 0);
signal sample_D_C    : std_logic_vector(31 downto 0);
signal ADC_SCLK_div  : std_logic_vector( 3 downto 0);
signal ADC_Tfirstclk : std_logic_vector( 4 downto 0);
signal sample_valid  : std_logic;
signal res           : std_logic;
signal SCLK          : std_logic;
signal DA            : std_logic_vector(3 downto 0);
signal DB            : std_logic_vector(3 downto 0);


attribute mark_debug : string;
attribute mark_debug of SCLK: signal is "true";
attribute mark_debug of DA: signal is "true";
attribute mark_debug of DB: signal is "true";
attribute mark_debug of sample_B_A: signal is "true";
attribute mark_debug of sample_D_C: signal is "true";
attribute mark_debug of conv_strobe: signal is "true";


begin

  res <= not s00_axi_aresetn;

-- Instantiation of Axi Bus Interface S00_AXI
QUAD_ADAQ23876_slave_lite_v1_0_S00_AXI_inst : QUAD_ADAQ23876_slave_lite_v1_0_S00_AXI
	generic map (
		C_S_AXI_DATA_WIDTH	=> C_S00_AXI_DATA_WIDTH,
		C_S_AXI_ADDR_WIDTH	=> C_S00_AXI_ADDR_WIDTH
	)
	port map (
    ADC_SCLK_div => ADC_SCLK_div,
    ADC_Tfirstclk => ADC_Tfirstclk,
    sample_B_A => sample_B_A,
    sample_D_C => sample_D_C,
    sample_valid => sample_valid,
		S_AXI_ACLK	=> s00_axi_aclk,
		S_AXI_ARESETN	=> s00_axi_aresetn,
		S_AXI_AWADDR	=> s00_axi_awaddr,
		S_AXI_AWPROT	=> s00_axi_awprot,
		S_AXI_AWVALID	=> s00_axi_awvalid,
		S_AXI_AWREADY	=> s00_axi_awready,
		S_AXI_WDATA	=> s00_axi_wdata,
		S_AXI_WSTRB	=> s00_axi_wstrb,
		S_AXI_WVALID	=> s00_axi_wvalid,
		S_AXI_WREADY	=> s00_axi_wready,
		S_AXI_BRESP	=> s00_axi_bresp,
		S_AXI_BVALID	=> s00_axi_bvalid,
		S_AXI_BREADY	=> s00_axi_bready,
		S_AXI_ARADDR	=> s00_axi_araddr,
		S_AXI_ARPROT	=> s00_axi_arprot,
		S_AXI_ARVALID	=> s00_axi_arvalid,
		S_AXI_ARREADY	=> s00_axi_arready,
		S_AXI_RDATA	=> s00_axi_rdata,
		S_AXI_RRESP	=> s00_axi_rresp,
		S_AXI_RVALID	=> s00_axi_rvalid,
		S_AXI_RREADY	=> s00_axi_rready
	);

	-- Add user logic here

  ADAQ23876_SPI_engine_inst: ADAQ23876_SPI_engine
    port map
      (
      reset             => res,
      clk               => s00_axi_aclk,
      -- conv clk is external
      convclk           => conv_strobe,
      ADC_SCLK_div      => ADC_SCLK_div,
      ADC_Tfirstclk     => ADC_Tfirstclk,
        -- lines to AXI interface
      sample_B_A        => sample_B_A,
      sample_D_C        => sample_D_C,
      sample_valid      => sample_valid,
      -- lines to SPI
      SCLK              => SCLK,
      DA                => DA,
      DB                => DB
      );

  -- LVDS buffers
  -- see IOBUF doc, which is UG974

  lvdsbuf_inst: for i in 0 to 3 generate
    CONV_STROBE_OBUFDS_inst : OBUFDS
    port map 
      (
      O  => cnv_p(i),
      OB => cnv_n(i),
      I  => conv_strobe
      );

    SCLK_OBUFDS_inst : OBUFDS
    port map 
      (
      O  => SCLK_p(i),
      OB => SCLK_n(i),
      I  => SCLK
      );

    DATA_A_BUF_inst : IBUFDS
    port map
      (
      O  => DA(i),
      I  => DA_p(i),
      IB => DA_n(i)
      );

    DATA_B_BUF_inst : IBUFDS
    port map
      (
      O  => DB(i),
      I  => DB_p(i),
      IB => DB_n(i)
      );

    end generate lvdsbuf_inst;


  QUAD_streamer_inst : QUAD_streamer
    port map
      (
      -- stream master
      M_AXIS_ACLK	     => M_AXIS_ACLK,
      M_AXIS_ARESETN	 => M_AXIS_ARESETN,
      M_AXIS_TVALID	   => M_AXIS_TVALID,
      M_AXIS_TDATA	   => M_AXIS_TDATA,
      M_AXIS_TSTRB	   => M_AXIS_TSTRB,
      M_AXIS_TKEEP	   => M_AXIS_TKEEP,
      M_AXIS_TLAST 	   => M_AXIS_TLAST,
      M_AXIS_TREADY	   => M_AXIS_TREADY,
  
      -- from SPI engine
      sample_B_A        => sample_B_A,
      sample_D_C        => sample_D_C,
      sample_valid      => sample_valid
      );


	-- User logic ends

end arch_imp;
