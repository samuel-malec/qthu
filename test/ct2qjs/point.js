function makePoint(x, y) {
    let p = {};
    p.x = x;
    p.y = y;
    return p;
}

function main() {
    let p = makePoint(3, 4);
    return p.x + p.y;
}
