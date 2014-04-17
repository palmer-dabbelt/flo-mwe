#include "tempdir.bash"

cat >test.scala <<EOF
import Chisel._

class test extends Module {
  val io = new Bundle {
    val i = UInt(INPUT,  width = 12)
    val o = UInt(OUTPUT, width = 16)
  }

  val iter = (0 until 4096).iterator
  val mem = Vec.fill(4096){UInt(iter.next, width = 16)}
  io.o := mem(io.i)
}

class tests(t: test) extends Tester(t) {
  for (cycle <- 0 until 4096) {
    poke(t.io.i, cycle % 4096)
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
