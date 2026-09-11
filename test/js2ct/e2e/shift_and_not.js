function run() {
    let a = 2 << 3;
    assert(a == 16);

    let b = 16 >> 2;
    assert(b == 4);

    let c = true;
    let d = !c;
    assert(d == false);
}

run();
