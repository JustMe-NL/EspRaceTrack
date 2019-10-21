
var connection = new WebSocket('ws://'+location.hostname+':81/', ['arduino']);
//var connection = new WebSocket('ws://192.168.7.31:81/', ['arduino']);
connection.onopen = function () {
    connection.send('Connect ' + new Date());
    readDrivers();
    readWifi();
};

connection.onerror = function (error) {
    console.log('WebSocket Error ', error);
};

connection.onmessage = function (e) {
    //console.log('Server: ', e.data);
    //document.getElementById('c').innerHTML = e.data;
    var mydata = e.data.split(":");
    if (e.data != mydata[0]) {
      if (mydata[0].charAt(0) == "S") {
        starttime = mydata[1];
        startRace();
      };
      if ((mydata[0] > 0) && (mydata[0] < 5)) {
        gotTime (mydata[0], mydata[1]);
      }
      if (mydata[0].charAt(0) == "X") {
        wifistatus = mydata[1];
        updatewifistatus();
      };
      if (mydata[0].charAt(0) == "R") {
        switch(mydata[0].charAt(1)) {
          case '1':
            document.getElementById("driver1").value = mydata[1];
            drivers[1] = mydata[1];
            break;
          case '2':
            document.getElementById("driver2").value = mydata[1];
            drivers[2] = mydata[1];
            break;
          case '3':
            document.getElementById("driver3").value = mydata[1];
            drivers[3] = mydata[1];
            break;
          case '4':
            document.getElementById("driver4").value = mydata[1];
            drivers[4] = mydata[1];
            break;
          case '5':
            document.getElementById("ssid").value = mydata[1];
            ssid = mydata[1];
            break;
          case '6':
            document.getElementById("pwd").value = mydata[1];
            pwd = mydata[1];
            break;
        };
      };
    };
};

connection.onclose = function(){
    console.log('WebSocket connection closed');
};

function sendStart() {
    console.log('S');
    connection.send('S');
};

function sendStop() {
    console.log('s');
    connection.send('s');
};

function readWifi() {
  connection.send('R5');
  connection.send('R6');
  connection.send('R7');
};

function readDrivers() {
  connection.send('R1');
  connection.send('R2');
  connection.send('R3');
  connection.send('R4');
}
