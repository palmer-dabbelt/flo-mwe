#include "tempdir.bash"
#include "chisel-jar.bash"

cat >test.scala <<EOF
import Chisel._

class test extends Module {
  val io = new Bundle {
    val a = UInt(INPUT, width = 24)
    val b = UInt(INPUT, width = 24)
    val p = UInt(OUTPUT, width = 48)
  }

  io.p := io.a * io.b
}

class tests(t: test) extends Tester(t) {
  for (cycle <- 0 until 300) {
    val a = BigInt(cycle) * cycle
    val b = BigInt(cycle) * cycle
    val p = a * b

    poke(t.io.a, a)
    poke(t.io.b, b)
    step(1)
    expect(t.io.p, p)
  }

  for (cycle <- 0 until 100) {
    val a = BigInt(24, rnd)
    val b = BigInt(24, rnd)
    val p = a * b

    poke(t.io.a, a)
    poke(t.io.b, b)
    step(1)
    expect(t.io.p, p)
  }
}

object test {
  def main(args: Array[String]): Unit = {
    chiselMainTest(args, () => Module(new test())) { t => new tests(t) }
  }
}
EOF

#include "harness.bash"
