@file:JvmName("HighLevelKotlin")
package uwvm.hll.kotlin

import java.nio.charset.StandardCharsets
import kotlin.system.exitProcess

private abstract class Accumulator(protected var value: Int) {
    abstract fun add(amount: Int): Int
}
private class Counter : Accumulator(40) {
    override fun add(amount: Int): Int { value += amount; return value }
}
private data class Reading(val name: String, val value: Int)
private sealed class Measurement {
    data class Valid(val reading: Reading) : Measurement()
    object Missing : Measurement()
}

private fun debugValue(): Int {
    var value = 40
    value += 2 // DEBUG_BEFORE
    return value // DEBUG_AFTER
}

private fun kernel(iterations: Int): Long {
    var value = 7
    for (index in 0 until iterations) value = (value * 33 + index) xor 0x55aa
    return value.toLong() and 0xffffffffL
}

fun main(args: Array<String>) {
    val iterations = if (args.isEmpty()) 1000 else args[0].toIntOrNull()
    if (args.size > 1 || iterations == null || iterations < 1 || iterations > 1000000) {
        System.err.println("kotlin invalid iterations")
        exitProcess(2)
    }
    val counter: Accumulator = Counter()
    val classes = counter.add(2)
    val numbers = (1..10).toList()
    val arrays = ArrayList(numbers).fold(0) { total, number -> total + number }
    val readings = listOf(Measurement.Valid(Reading("joint", 1)), Measurement.Missing,
        Measurement.Valid(Reading("joint", 5)))
    val records = readings.sumOf { when (it) {
        is Measurement.Valid -> if (it.reading.name == "joint") it.reading.value else 0
        Measurement.Missing -> 0
    } }
    val text = "π你好🙂"
    var exceptions = 0
    try { throw IllegalStateException("expected") }
    catch (error: IllegalStateException) { if (error.message == "expected") exceptions++ }
    try { numbers[classes] }
    catch (error: IndexOutOfBoundsException) { exceptions++ }
    try { java.lang.Integer.parseInt("bad") }
    catch (error: NumberFormatException) { exceptions++ }
    val value = debugValue()
    check(classes == 42 && arrays == 55 && records == 6 && text.length == 5 &&
        text.toByteArray(StandardCharsets.UTF_8).size == 12 && exceptions == 3 && value == 42)
    println("kotlin classes=$classes arrays=$arrays records=$records utf16=5 utf8=12 exceptions=$exceptions value=$value iterations=$iterations checksum=${kernel(iterations)}")
}
