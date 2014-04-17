#include "tempdir.bash"
#include "chisel-jar.bash"

cat >test.scala <<EOF
import Chisel._

class test extends Module {
  val io = new Bundle {
    val i = UInt(INPUT, width = 48)
    val o = Vec.fill(42){ UInt(OUTPUT, width = 6) }
  }

  for (i <- 0 until 42) { io.o(i) := io.i >> UInt(i) }
}

class tests(t: test) extends Tester(t) {
  for (i <- 1 until 100) {
    poke(t.io.i, BigInt(48, rnd))
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
