"""Independent scalar formulation; execute only in the admitted Linux cgroup.

This deliberately does not import the guest fixture or its Accumulator class.
Reference runs must also execute the original fixtures with native CPython.
"""
import sys


def checksum(rounds):
    slots = [(7 * index + 3) % 97 for index in range(64)]
    total = 0
    for outer in range(rounds):
        offset = outer % 64
        adjustment = outer % 13
        for inner in range(64):
            position = (offset + inner) % 64
            updated = (17 * slots[position] + inner + adjustment) % 997
            slots[position] = updated
            total = (total + updated * (1 + inner % 11)) % 1_000_003
    return total


if __name__ == "__main__":
    for argument in sys.argv[1:]:
        count = int(argument)
        if not 1 <= count <= 100_000_000:
            raise ValueError("iterations out of range")
        print(f"iterations={count} checksum={checksum(count)}")
