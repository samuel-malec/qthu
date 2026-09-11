function run(n) {
    let i = 0;
    let total = 0;
    while (i < n) {
        if (i < 5) {
            total = total + i;
        } else {
            total = total - i;
        }
        i = i + 1;
    }
    return total;
}
