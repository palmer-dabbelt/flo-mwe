#include "tempdir.bash"
#include "chisel-jar.bash"

cat >test.scala <<EOF
import Chisel._

class test extends Module {
  val io = new Bundle {
    val o = UInt(OUTPUT, width = 250)
  }

  val r = Reg(init = UInt(0, width = 250))
  r := r ^ r
  io.o := r
}

class tests(t: test) extends Tester(t) {
  var cycle = 0
  do {
    step(1)
    cycle += 1
  } while (cycle < 10)
}

object test {
  def main(args: Array[String]): Unit = {
    chiselMainTest(args, () => Module(new test())) { t => new tests(t) }
  }
}
EOF

#include "harness.bash"
