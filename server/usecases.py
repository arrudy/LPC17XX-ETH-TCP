import random

def func_runner(func_code):
    match func_code:
        case "getrandom\0":
            return str(random.randint(0,10))
        case "ta energy\0":
            return str()
        case "ta casino\0":
            l1 = random.randint(0,10)
            l2 = random.randint(0,10)
            l3 = random.randint(0,10)
            if l1 == l2 and l1 == l2:
                energy+=1
            return str(f"{l1}|{l2}|{l3}")
        case "ta ad\0"
            pass
           
        case _:
            pass        
    