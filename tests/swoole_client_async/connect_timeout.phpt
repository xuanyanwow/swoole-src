--TEST--
swoole_client_async: connect_host_not_found
--SKIPIF--
<?php require  __DIR__ . '/../include/skipif.inc'; ?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

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

$cli->connect("192.0.0.1", 9000, 0.1);
?>
--EXPECT--
error
