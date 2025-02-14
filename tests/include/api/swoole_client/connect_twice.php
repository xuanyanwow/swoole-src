<?php

$start = microtime(true);

$cli = new Swoole\Async\Client(SWOOLE_SOCK_TCP);
$cli->on("connect", function(Swoole\Async\Client $cli) {
    Assert::true(false, 'never here');
});
$cli->on("receive", function(Swoole\Async\Client $cli, $data) {
    Assert::true(false, 'never here');
});
$cli->on("error", function(Swoole\Async\Client $cli) {
    echo "error\n";
});
$cli->on("close", function(Swoole\Async\Client $cli) {
    echo "close\n";
});

function refcount($var)
{
    ob_start();
    debug_zval_dump($var);
    preg_match('/refcount\((?<refcount>\d)\)/', ob_get_clean(), $matches);
    return intval($matches["refcount"]) - 3;
}

@$cli->connect("11.11.11.11", 9000, 0.1);
@$cli->connect("11.11.11.11", 9000, 0.1);
@$cli->connect("11.11.11.11", 9000, 0.1);
@$cli->connect("11.11.11.11", 9000, 0.1);
@$cli->connect("11.11.11.11", 9000, 0.1);
Swoole\Event::wait();
// xdebug_debug_zval("cli");
// echo refcount($cli); // php7无效
