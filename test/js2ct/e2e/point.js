function makePoint(x, y) {
    let p = {};
    p.x = x;
    p.y = y;
    return p;
}

function run() {
    let p = makePoint(3, 4);
    assert(p.x + p.y == 7);
}

run();
