function sideEffect(y) {
    return y + 1;
}

function run() {
    assert((true && true) == true);
    assert((true && false) == false);
    assert((0 && 2) == 0);
    assert((1 && 2) == 2);
    assert((0 || 5) == 5);
    assert((1 || 5) == 1);
    assert(((1 && 0) || 3) == 3);

    let x = 0;
    let y = 5;
    let r = x && sideEffect(y);
    assert(y == 5);
    assert(r == 0);

    let a = 1;
    let b = 2;
    let c = a && (b = 99);
    assert(b == 99);
    assert(c == 99);
}

run();
