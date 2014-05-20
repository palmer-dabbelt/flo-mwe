#include "tempdir.bash"
#include "chisel-jar.bash"

cat >test.scala <<EOF
import Chisel._

class test extends Module {
  val io = new Bundle {
    val i = UInt(INPUT, width = 48)
    val o = Vec.fill(8) { UInt(OUTPUT, width = 48) }
  }

  for (i <- 1 until 8) {
    io.o(i) := io.i << UInt(i)
  }
}

class tests(t: test) extends Tester(t) {
  for (cycle <- 0 until 100) {
    val i = BigInt(24, rnd)

    poke(t.io.i, i)
    step(1)

    for (o <- 1 until 8) {
      expect(t.io.o(o), i << o)
    }
  }
}

object test {
  def main(args: Array[String]): Unit = {
    chiselMainTest(args, () => Module(new test())) { t => new tests(t) }
  }
}
EOF

#include "harness.bash"
