function makeBox() {
    let b = {};
    let items = [];
    items[0] = 100;
    items[1] = 200;
    b.items = items;
    return b;
}

function main() {
    let b = makeBox();
    let items = b.items;
    return items[0] + items[1];
}
