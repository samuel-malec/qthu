function sumTo(n) {
    let i = 0;
    let s = 0;
    while (i < n) {
        s = s + i;
        i = i + 1;
    }
    return s;
}

let result = sumTo(5);
