#include "tempdir.bash"
#include "chisel-jar.bash"

cat >test.scala <<EOF
import Chisel._

class test extends Module {
  val io = new Bundle {
    val i = UInt(INPUT,  width = 64)
    val o =  UInt(OUTPUT, width = 64)
  }

  io.o := ~io.i
}

class tests(t: test) extends Tester(t) {
  for (cycle <- 0 until 10) {
    poke(t.io.i, BigInt(64, rnd))
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
