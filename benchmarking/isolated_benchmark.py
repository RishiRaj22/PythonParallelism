import time
import logging

def py_factorial(n):
    begin = time.time()
    k = 1
    for i in range(n,1,-1):
        k *= i
        k %= 1000000007
    logging.warning(f"[PY_FACTORIAL] py_factorial function iternals took {(time.time() - begin)*1000} ms")
    return k
