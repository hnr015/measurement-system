<?php
$servername = "localhost"; // Adres serwera MySQL
$username = "root"; // Nazwa użytkownika MySQL
$password = ""; // Hasło MySQL
$dbname = "system_pomiarowydb"; // Nazwa bazy danych

// Utwórz połączenie
$conn = new mysqli($servername, $username, $password, $dbname);

// Sprawdź połączenie
if ($conn->connect_error) {
  die("Błąd połączenia: " . $conn->connect_error);
}

// Pobierz dane JSON
$jsonData = file_get_contents('php://input');
$data = json_decode($jsonData, true);

// ustaw datę i godzinę
date_default_timezone_set("Europe/Warsaw");
$time = date("H:i:s");
$date = date("Y-m-d");

$sql = "INSERT INTO dane_pomiarowe (id, temperature, humidity, lpg, co, smoke, time, date) VALUES ('".$data["id"]."', '".$data["t"]."', '".$data["h"]."', '".$data["lpg"]."', '".$data["co"]."', '".$data["smoke"]."', '$time', '$date')";

if ($conn->query($sql) === TRUE) {
  echo "Dane zapisane pomyślnie";
} else {
  echo "Błąd: " . $sql . "<br>" . $conn->error;
}

$conn->close();
?>