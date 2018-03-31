#!/usr/bin/php
<?php

$t = file_get_contents('Makefile');
{
  $t = str_replace("BINPATH = \r\n", "BINPATH = /usr/bin\r\n", $t);
  
  $t = preg_replace_callback('/C_SOURCES =.*?\r\n\r\n/s', function($m) {
    $x = trim($m[0]);
    $lines = array_map('trim', explode('\\', $x));
    $lines = array_slice($lines, 1);
    $lines = array_unique($lines);
    $t2 = "C_SOURCES = \\\r\n" . implode(" \\\r\n", $lines);
    return $t2."\r\n\r\n";
  }, $t);
}
file_put_contents('Makefile', $t);

/*
$t = file_get_contents("Src/freertos.c");
{
  $t = str_replace('__weak void', 'void __attribute__((weak))', $t);
}
file_put_contents("Src/freertos.c", $t);
*/
