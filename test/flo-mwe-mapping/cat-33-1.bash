#include "tempdir.bash"
#include "chisel-jar.bash"

cat >test.scala <<EOF
import Chisel._

class test extends Module {
  val io = new Bundle {
    val i0 = UInt(INPUT, width = 33)
    val i1 = UInt(INPUT, width = 1)

    val o =  UInt(OUTPUT)
  }

  io.o := Cat(io.i0, io.i1)
}

class tests(t: test) extends Tester(t) {
  for (cycle <- 0 until 33) {
    poke(t.io.i0, BigInt(1) << cycle)
    poke(t.io.i1, 0)
    step(1)

    poke(t.io.i0, BigInt(1) << cycle)
    poke(t.io.i1, 1)
    step(1)
  }

  for (cycle <- 0 until 1000) {
    poke(t.io.i0, BigInt(33, rnd))
    poke(t.io.i1, BigInt(1,  rnd))
    step(1)
  }
}

object test {
  def main(args: Array[String]): Unit = {
    chiselMainTest(args, () => Module(new test())) { t => new tests(t) }
  }
}
EOF

#include "harness.bash"
