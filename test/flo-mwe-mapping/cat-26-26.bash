#include "tempdir.bash"
#include "chisel-jar.bash"

cat >test.scala <<EOF
import Chisel._

class test extends Module {
  val io = new Bundle {
    val i0 = UInt(INPUT, width = 26)
    val i1 = UInt(INPUT, width = 26)

    val o =  UInt(OUTPUT)
  }

  io.o := Cat(io.i0, io.i1)
}

class tests(t: test) extends Tester(t) {
  for (cycle <- 0 until 10) {
    poke(t.io.i0, rnd.nextInt(67108864))
    poke(t.io.i1, rnd.nextInt(67108864))
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
