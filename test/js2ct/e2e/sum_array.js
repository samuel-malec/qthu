function sumArray(arr, n) {
    let i = 0;
    let total = 0;
    while (i < n) {
        total = total + arr[i];
        i = i + 1;
    }
    return total;
}

function run() {
    let a = [];
    a[0] = 10;
    a[1] = 20;
    a[2] = 30;
    assert(sumArray(a, 3) == 60);
}

run();
