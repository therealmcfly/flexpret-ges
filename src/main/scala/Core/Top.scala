package flexpret.core
import chisel3._
import chisel3.util.MixedVec
import chisel3.util.experimental.loadMemoryFromFileInline // Load the contents of ISPM from file
import flexpret.{WishboneBus, WishboneMaster, WishboneUart}

abstract class AbstractTop(cfg: FlexpretConfiguration, cfgHash: UInt) extends Module {
    val core = Module(new Core(cfg, cfgHash))

    val wbMaster = Module(new WishboneMaster(cfg.busAddrBits)(cfg))

		// // Original instantiation with single UART
    // val wbUart   = Module(new WishboneUart()(cfg)) // Original single UART
    // val wbBus    = Module(new WishboneBus(cfg.busAddrBits, Seq(4))) // Original bus with single device (UART)

		// New instantiation with two UARTs
		val wbUart0  = Module(new WishboneUart()(cfg)) // Rename UART to UART0
		val wbUart1  = Module(new WishboneUart()(cfg)) // New UART (UART1)
		val wbBus    = Module(new WishboneBus(cfg.busAddrBits, Seq(4, 4))) // Update bus to have 2 devices (UART0 and UART1) each with address width 4

    // Connect WB bus to FlexPRET bus
    wbMaster.busIO <> core.io.bus

    // Connect WB bus to WB master
    wbBus.io.wbMaster <> wbMaster.wbIO

    // // Connect WB bus to WB UART
    // wbBus.io.wbDevices(0) <> wbUart.io.port // Original single UART connection

		// New connections for two UARTs
		wbBus.io.wbDevices(0) <> wbUart0.io.port // Connect first UART to first bus device
		wbBus.io.wbDevices(1) <> wbUart1.io.port // Connect second UART to second bus device
} 

class VerilatorTopIO(cfg: FlexpretConfiguration) extends Bundle {
    val gpio = new GPIO()(cfg)
    val uart = new Bundle {
        val rx = Input(Bool())
        val tx = Output(Bool())
    }
    val to_host = Output(Vec(cfg.threads, UInt(32.W)))
    val int_exts = Input(Vec(cfg.threads, Bool()))
    val imem_store = Output(Bool())
}

class VerilatorTop(cfg: FlexpretConfiguration, cfgHash: UInt) extends AbstractTop(cfg, cfgHash) {
    val io = IO(new VerilatorTopIO(cfg))
    val regPrintNext = RegInit(VecInit(Seq.fill(cfg.threads) {false.B} ))

    io.gpio <> core.io.gpio

    // Connect rx tx signals
    io.uart.tx := wbUart0.ioUart.tx
    wbUart0.ioUart.rx := io.uart.rx
    
    core.io.int_exts <> io.int_exts

    // Drive bus input to 0
    core.io.dmem.driveDefaultsFlipped()
    core.io.imem_bus.driveDefaultsFlipped()

    // Catch termination from core
    for (tid <- 0 until cfg.threads) {
        io.to_host(tid) := core.io.host.to_host(tid)
    }

    io.imem_store := core.io.imem_store
}

class FpgaTopIO(cfg: FlexpretConfiguration) extends Bundle {
    val gpio = new GPIO()(cfg)
    val uart = new Bundle {
        val rx = Input(Bool())
        val tx = Output(Bool())
    }
		// New UART interface
		val uart1 = new Bundle {
			val rx = Input(Bool())
			val tx = Output(Bool())
		}
		
    val int_exts = Input(Vec(cfg.threads, Bool()))
}

class FpgaTop(cfg: FlexpretConfiguration, cfgHash: UInt) extends AbstractTop(cfg, cfgHash) {
    val io = IO(new FpgaTopIO(cfg))
    
    io.gpio <> core.io.gpio
    core.io.int_exts <> io.int_exts

    // Connect rx tx signals for UART0
    io.uart.tx := wbUart0.ioUart.tx
    wbUart0.ioUart.rx := io.uart.rx

		// Connect rx tx signals for New UART1 
		io.uart1.tx := wbUart1.ioUart.tx
		wbUart1.ioUart.rx := io.uart1.rx

    // Drive bus input to 0
    core.io.dmem.driveDefaultsFlipped()
    core.io.imem_bus.driveDefaultsFlipped()
}
