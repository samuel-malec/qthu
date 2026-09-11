function run() {
    let a = 6;
    let b = 3;

    assert((a & b) == 2);
    assert((a | b) == 7);
    assert((a ^ b) == 5);
    assert((~a + 7) == 0);
}

run();
