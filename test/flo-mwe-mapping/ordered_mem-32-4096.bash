#include "tempdir.bash"

cat >test.scala <<EOF
import Chisel._

class test extends Module {
  val io = new Bundle {
    val r = Bool(INPUT)
    val i = UInt(INPUT,  width = 12)
    val o = UInt(OUTPUT, width = 16)
  }

  val mem = Mem(UInt(width = 16), 4096)

  val r = Reg(init = UInt(0, width = 16))
  r := r + UInt(1)

  io.o := UInt(1)
  when (io.r)  { io.o := mem(io.i) }
  when (!io.r) { mem(io.i) := r }
}

class tests(t: test) extends Tester(t) {
  for (cycle <- 0 until 4096) {
    poke(t.io.i, cycle % 4096)
    poke(t.io.r, 0)
    step(1)
  }

  for (cycle <- 0 until 4096) {
    poke(t.io.i, cycle % 4096)
    poke(t.io.r, 1)
    step(1)
  }
}

object test {
  def main(args: Array[String]): Unit = {
    chiselMainTest(args, () => Module(new test())) { t => new tests(t) }
  }
}
EOF

#include "chisel-jar.bash"
#include "harness.bash"
