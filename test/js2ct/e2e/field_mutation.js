function run() {
    let obj = {};
    obj.x = 1;
    obj.x = obj.x + 1;
    assert(obj.x == 2);
}

run();
