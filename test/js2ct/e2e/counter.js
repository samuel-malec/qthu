function makeCounter() {
    let c = {};
    c.count = 0;
    return c;
}

function incrementBy(c, n) {
    let i = 0;
    while (i < n) {
        c.count = c.count + 1;
        i = i + 1;
    }
    return c;
}

function run() {
    let c = makeCounter();
    c = incrementBy(c, 5);
    assert(c.count == 5);
}

run();
