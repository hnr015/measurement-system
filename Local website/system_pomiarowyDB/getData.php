<?php
$servername = "localhost"; // Adres serwera MySQL
$username = "root"; // Nazwa użytkownika MySQL
$password = ""; // Hasło MySQL
$dbname = "system_pomiarowydb"; // Nazwa bazy danych

$conn = new mysqli($servername, $username, $password, $dbname);

if ($conn->connect_error) {
  die("Connection failed: " . $conn->connect_error);
}



// Pobierz ostatnie 10 pomiarów
$sql = "SELECT temperature, humidity, lpg, co, smoke, DATE(date) AS data, TIME(time) AS time FROM dane_pomiarowe ORDER BY date DESC, time DESC LIMIT 10";
$result = $conn->query($sql);

$data = array();
if ($result->num_rows > 0) {
  while($row = $result->fetch_assoc()) {
    $row['datetime'] = $row['data'] . ' ' . $row['time'];
    $data[] = $row;
  }
}
// Zwróć dane w formacie JSON
header('Content-Type: application/json');
echo json_encode($data);

$conn->close();
?>
