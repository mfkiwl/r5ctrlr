library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
library UNISIM;
use UNISIM.VComponents.all;


entity AD3552_SPI is
	generic (
		-- Users to add parameters here

		-- User parameters ends
		-- Do not modify the parameters beyond this line


		-- Parameters of Axi Slave Bus Interface S00_AXI
		C_S00_AXI_DATA_WIDTH	: integer	:= 32;
		C_S00_AXI_ADDR_WIDTH	: integer	:= 6
	);
	port (
		-- Users to add ports here
    SPI_CSn           : out std_logic;
    SPI_SCLK          : out std_logic;
    XLATOR_DIR        : out std_logic; -- '1' means input to FPGA
    DAC_HW_RESETn     : out std_logic;
    SPI_SDIO          : inout std_logic_vector(3 downto 0);

		-- User ports ends
		-- Do not modify the ports beyond this line


		-- Ports of Axi Slave Bus Interface S00_AXI
		s00_axi_aclk	: in std_logic;
		s00_axi_aresetn	: in std_logic;
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
end AD3552_SPI;

architecture arch_imp of AD3552_SPI is

	-- component declaration
	component AD3552_SPI_slave_lite_v1_0_S00_AXI is
		generic (
		C_S_AXI_DATA_WIDTH	: integer	:= 32;
		C_S_AXI_ADDR_WIDTH	: integer	:= 6
		);
		port (
		-- lines to command interpreter
		start_transaction : out std_logic;
		busy              :  in std_logic;
    DAC_soft_reset    : out std_logic;
    DAC_hw_resetn     : out std_logic;
		DAC_SPI_CLK_div   : out std_logic_vector(1 downto 0);
		DAC_RW            : out std_logic;
		DAC_addr_data     : out std_logic;
		DAC_LAST          : out std_logic;
		DAC_bytenum       : out std_logic_vector(1 downto 0);
		DAC_wr_word       : out std_logic_vector(23 downto 0);
		DAC_rd_word       :  in std_logic_vector(23 downto 0);
		errcode           :  in std_logic_vector(3 downto 0);
        -------------------------------
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
	end component AD3552_SPI_slave_lite_v1_0_S00_AXI;

	component AD3552_CMD_interpreter is
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
	end component AD3552_CMD_interpreter;

	component AD3552_SPI_engine is
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
		  -- lines to SPI
		  XLATOR_DIR        : out std_logic; -- '1' means input to FPGA
		  SCLK              : out std_logic;
		  SDIO_i            :  in std_logic_vector(3 downto 0);
		  SDIO_o            : out std_logic_vector(3 downto 0);
		  SDIO_t            : out std_logic_vector(3 downto 0)
		  );
	end component AD3552_SPI_engine;

signal DAC_CMD_reset     : std_logic;
signal DAC_soft_reset     : std_logic;
signal start_transaction : std_logic;
signal busy              : std_logic;
signal DAC_SPI_CLK_div   : std_logic_vector(1 downto 0);
signal DAC_RW            : std_logic;
signal DAC_addr_data     : std_logic;
signal DAC_LAST          : std_logic;
signal DAC_bytenum       : std_logic_vector(1 downto 0);
signal DAC_wr_word       : std_logic_vector(23 downto 0);
signal DAC_rd_word       : std_logic_vector(23 downto 0);
signal errcode           : std_logic_vector(3 downto 0);
signal SPI_reset         : std_logic;
signal SPI_CE            : std_logic;
signal SPI_wbyte         : std_logic_vector(7 downto 0);
signal SPI_rbyte         : std_logic_vector(7 downto 0);
signal SPI_RW            : std_logic;
signal SPI_start         : std_logic;
signal SPI_busy          : std_logic;
signal SDIO_i            : std_logic_vector(3 downto 0);
signal SDIO_o            : std_logic_vector(3 downto 0);
signal SDIO_t            : std_logic_vector(3 downto 0);

begin

	DAC_CMD_reset <= not s00_axi_aresetn or DAC_soft_reset;

-- Instantiation of Axi Bus Interface S00_AXI
AD3552_SPI_slave_lite_v1_0_S00_AXI_inst : AD3552_SPI_slave_lite_v1_0_S00_AXI
	generic map (
		C_S_AXI_DATA_WIDTH	=> C_S00_AXI_DATA_WIDTH,
		C_S_AXI_ADDR_WIDTH	=> C_S00_AXI_ADDR_WIDTH
	)
	port map (
		-- lines to command interpreter
		start_transaction => start_transaction,
		busy              => busy,
    DAC_soft_reset    => DAC_soft_reset,
    DAC_hw_resetn     => DAC_HW_RESETn,
		DAC_SPI_CLK_div   => DAC_SPI_CLK_div,
		DAC_RW            => DAC_RW,
		DAC_addr_data     => DAC_addr_data,
		DAC_LAST          => DAC_LAST,
		DAC_bytenum       => DAC_bytenum,
		DAC_wr_word       => DAC_wr_word,
		DAC_rd_word       => DAC_rd_word,
		errcode           => errcode,
		-------------------------------
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

    -- instantiation of command interpreter
    AD3552_CMD_interpreter_i : AD3552_CMD_interpreter
      port map
        (
        reset             => DAC_CMD_reset,
        clk               => s00_axi_aclk,
        -- lines to the register bank
        start_transaction => start_transaction,
        busy              => busy,
        DAC_SPI_CLK_div   => DAC_SPI_CLK_div,
        DAC_RW            => DAC_RW,
        DAC_addr_data     => DAC_addr_data,
        DAC_LAST          => DAC_LAST,
        DAC_bytenum       => DAC_bytenum,
        DAC_wr_word       => DAC_wr_word,
        DAC_rd_word       => DAC_rd_word,
        errcode           => errcode,
        -- lines to the SPI engine
        SPI_reset         => SPI_reset,
        SPI_CE            => SPI_CE,
        SPI_wbyte         => SPI_wbyte,
        SPI_rbyte         => SPI_rbyte,
        SPI_RW            => SPI_RW,
        SPI_start         => SPI_start,
        SPI_busy          => SPI_busy,
        -- output lines
        SPI_CSn           => SPI_CSn
        );

    -- instantiation of SPI engine
    AD3552_SPI_engine_i : AD3552_SPI_engine
      port map
        (
        reset             => SPI_reset,
        clk               => s00_axi_aclk,
        ce                => SPI_CE,
        -- lines to command interpreter
        start_transaction => SPI_start,
        busy              => SPI_busy,
        wbyte             => SPI_wbyte,
        rbyte             => SPI_rbyte,
        RW                => SPI_RW,
        -- lines to SPI
        XLATOR_DIR        => XLATOR_DIR,
        SCLK              => SPI_SCLK,
        SDIO_i            => SDIO_i,
        SDIO_o            => SDIO_o,
        SDIO_t            => SDIO_t
        );

    -- instantiation of quadSPI IOBUF
    SPI_IOBUF_array_i : for i in 0 to 3 generate
      IOBUF_i : IOBUF
        port map
          (
          I  => SDIO_i(i),
          O  => SDIO_o(i),
          T  => SDIO_t(i),
          IO => SPI_SDIO(i)
          );
    end generate SPI_IOBUF_array_i;
    

    -- User logic ends

end arch_imp;
