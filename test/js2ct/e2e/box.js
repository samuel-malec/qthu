function makeBox() {
    let b = {};
    let items = [];
    items[0] = 100;
    items[1] = 200;
    b.items = items;
    return b;
}

function run() {
    let b = makeBox();
    let items = b.items;
    assert(items[0] + items[1] == 300);
}

run();
