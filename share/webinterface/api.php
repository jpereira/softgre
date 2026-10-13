<?php

/* Read-only SoftGRE TCP adapter; serve only on a trusted management network. */
declare(strict_types=1);

header('Content-Type: application/json; charset=utf-8');
header('Cache-Control: no-store');
header('X-Content-Type-Options: nosniff');

function respond(int $code, string $status, string $body): never
{
    http_response_code($code);
    echo json_encode(['status' => $status, 'body' => $body], JSON_THROW_ON_ERROR);
    exit;
}
if ($_SERVER['REQUEST_METHOD'] !== 'GET') {
    header('Allow: GET');
    respond(405, 'INVALID', 'Use GET');
}
$commands = ['status' => 'STAT', 'tunnels' => 'STUN', 'ues' => 'LMIP',
             'cpe' => 'GTMC', 'version' => 'VERS', 'help' => 'HELP'];
$action = $_GET['action'] ?? 'status';
$arg = $_GET['arg'] ?? '';
if (!is_string($action) || !isset($commands[$action]) || !is_string($arg)) {
    respond(400, 'INVALID', 'Invalid action or argument');
}
if ($action === 'ues' && filter_var($arg, FILTER_VALIDATE_IP, FILTER_FLAG_IPV4) === false) {
    respond(400, 'INVALID', 'Expected an IPv4 address');
}
if ($action === 'cpe' && !preg_match('/^[0-9a-fA-F]{2}(:[0-9a-fA-F]{2}){5}$/D', $arg)) {
    respond(400, 'INVALID', 'Expected a colon-separated MAC address');
}
$host = getenv('SOFTGRE_API_HOST') ?: '127.0.0.1';
$port = getenv('SOFTGRE_API_PORT') ?: '8888';
if (
    filter_var($host, FILTER_VALIDATE_IP, FILTER_FLAG_IPV4) === false ||
    !ctype_digit($port) || (int)$port < 1 || (int)$port > 65535
) {
    respond(500, 'ERROR', 'Invalid backend configuration');
}
$socket = @stream_socket_client("tcp://$host:$port", $errno, $error, 2);
if ($socket === false) {
    respond(502, 'ERROR', 'Backend unavailable');
}
stream_set_timeout($socket, 5);
$command = $commands[$action];
if ($action === 'ues' || $action === 'cpe') {
    $command .= ' ' . $arg;
}
$command .= "\n";
$sent = 0;
while ($sent < strlen($command)) {
    $written = fwrite($socket, substr($command, $sent));
    if ($written === false || $written === 0) {
        fclose($socket);
        respond(502, 'ERROR', 'Backend write failed');
    }
    $sent += $written;
}
$reply = '';
$deadline = microtime(true) + 5;
while (!feof($socket)) {
    $remaining = $deadline - microtime(true);
    if ($remaining <= 0) {
        fclose($socket);
        respond(504, 'ERROR', 'Backend timeout');
    }
    stream_set_timeout($socket, (int)$remaining, (int)(($remaining - (int)$remaining) * 1000000));
    $chunk = fread($socket, 8192);
    if ($chunk === false || stream_get_meta_data($socket)['timed_out']) {
        fclose($socket);
        respond(504, 'ERROR', 'Backend timeout');
    }
    $reply .= $chunk;
    if (strlen($reply) > 1024 * 1024) {
        fclose($socket);
        respond(502, 'ERROR', 'Backend response too large');
    }
}
fclose($socket);
if ($action === 'help') {
    respond(200, 'OK', $reply);
}
if (!preg_match('/^RESULT: (OK|NULL|NOTFOUND|INVALID)\n(?:BODY: ([\s\S]*))?$/D', $reply, $match)) {
    respond(502, 'ERROR', 'Invalid backend response');
}
respond(200, $match[1], rtrim($match[2] ?? '', "\r\n"));
