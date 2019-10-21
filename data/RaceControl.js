var currentLap = 0;
var raceActive = 0;          // 0 = reset, 1 = start, 2 = active, 3 = stopped
var raceStartLights = 0;
var starttime = 0;
var timer;
var lanes = [0, -1, -1, -1, -1];
var fastestLap;
var leadertime;
var maxLaps = 5;
var drivers = ["", "Lane A", "Lane B", "Lane C", "Lane D"];
var ssid = "";
var pwd = "";

document.addEventListener('readystatechange', event => {
  if (event.target.readyState === "complete") {
      document.getElementById("wait").style.display = "none";
    }
});

function setupApp() {
  if (document.getElementById("settings").style.display == "none") {
    readWifi();
    readDrivers();
    for (i = 1;i < 5;i++) {
      document.getElementById("driver" + i).value = drivers[i];
    };
    document.getElementById("maxLaps").value = maxLaps;
    document.getElementById("settings").style.display = "block";
  } else {
    document.getElementById("settings").style.display = "none";
  }
};

function updatewifistatus() {
  document.getElementById("wifistatus").innerHTML = wifistatus;
};

function setDrivers() {
  var cells = document.getElementById("topLine").children;
  for (i = 1; i < 5; i++) {
    cells[i].innerHTML = drivers[i];
  };
};

function startPushed() {
  if ((raceActive == 1) || (raceActive == 2)) {
    stopRace();
  } else {
    if (raceActive == 0) {
      raceActive = 1;
      timer = setInterval(startLights,1000);
      setDrivers();
      document.getElementById("start").innerHTML = "Stop";
      document.getElementById("start").style.backgroundColor = "#f46542";
    } else {
      resetScreen();
      raceActive = 0;
    };
  };
};

function startLights() {
  raceStartLights++;
  if (raceStartLights == 6) {
    clearInterval(timer);
    for (i = 1; i < 5; i++) {
      lanes[i] = -1;
    };
    lanes[0] = 0;
    sendStart();
  } else {
    var myElememt = "d" + raceStartLights
    document.getElementById(myElememt).style.backgroundColor = "red";
    document.getElementById(myElememt + "a").style.backgroundColor = "red";
  };
};

function stopRace() {
  raceActive = 3;
  clearInterval(timer);
  document.getElementById("start").innerHTML = "Reset";
  document.getElementById("start").style.backgroundColor = "#00878F";
};

function startRace() {
  raceActive = 2;
  fastestlap = 4294967295;
  for (i = 1; i < 6; i++) {
    var myElememt = "d" + i;
    document.getElementById(myElememt).style.backgroundColor = "#bbb";
    document.getElementById(myElememt + "a").style.backgroundColor = "#bbb";
  };
  raceStartLights = 0;
};

function calcLap(time1, time2, track) {
  var laptime = time2 - time1;
  var min = Math.floor((laptime/1000/60) << 0);
  var sec = Math.floor((laptime/1000) % 60);
  var hun = Math.floor(laptime/10);
  var lapstr = ("00" + min).slice (-2) + ":" + ("00" + sec).slice (-2) + "." + ("00" + hun).slice (-2);
  if ((laptime < fastestlap) || (fastestlap == 0)) {
    fastestlap = laptime;
    document.getElementById("fastestLap1").innerHTML = drivers[track]
    document.getElementById("fastestLap2").innerHTML = lapstr;
  };
  return lapstr;
};

function gotTime(lane, time) {
  if (raceActive > 1) {
    var row;
    var cell;
    var cells;
    lanes[lane]++;
    if ((lanes[lane] > 0) && (lanes[lane] <= maxLaps)){ // Ignore 1st trigger and victory laps
      if (lanes[lane] > currentLap) {
        currentLap++;
        leadertime = time;
        document.getElementById("currentLap").innerHTML = currentLap + " / " + maxLaps;
        row = document.getElementById("lapTimes").insertRow(1);
        row.setAttribute("id", "lap" + currentLap);
        cell = row.insertCell(0);
        cell.innerHTML = "Lap " + currentLap;
        for (i = 1; i < 5; i++) {
          cell = row.insertCell(i);
          if (i == lane) {
            cell.setAttribute("ms", time);
            if (currentLap == 1) {
              cell.innerHTML = calcLap(starttime, time, lane);
              document.getElementById("liveTrack").style.display = "block";
              document.getElementById("startLights").style.display = "none";
            } else {
              cells = document.getElementById("lap" + (currentLap - 1)).children;
              cell.innerHTML = calcLap(cells[lane].getAttribute("ms"), time, lane);
            } // currentLap == 1
          }; // if i==lane
        }; // for i
        document.getElementById("live1").innerHTML = drivers[lane];
        document.getElementById("live1").innerHTML = drivers[lane];
        if (currentLap >= maxLaps) {
          finishRace(lane, time);
        } // finish?
      } else { // not new lap?
        if (lanes[lane] <= maxLaps) {
          cell = document.getElementById("lap" + lanes[lane]).children;
          cell[lane].setAttribute("ms", time);
          if (lanes[lane] == 1) {
            cell[lane].innerHTML = calcLap(starttime, time, lane);
          } else {
            cells = document.getElementById("lap" + (lanes[lane] - 1)).children;
            cell[lane].innerHTML = calcLap(cells[lane].getAttribute("ms"), time, lane);
          }; // 1st vs other laps
          var myplace = 0;
          for (i = 1; i < 5; i++) {
            if (lanes[i] >= lanes[lane]) {
              myplace++;
            };
          };
          document.getElementById("live" + myplace).innerHTML = drivers[lane];
          document.getElementById("live1" + myplace).innerHTML = "+ " + calcLap(leadertime, time, lane);
        }; // until finish
      }; // new lap
    }; // ignore 1st trigger
  } else {
    if (raceActive == 1) {
      stopRace();
      document.getElementById("jumpStart").style.display = "block";
      document.getElementById("violator").innerHTML = drivers[lane] + " jumped the start, race halted.";
    };
  }; // jumpstart
};

function finishRace(winner, laptime) {
  document.getElementById("finishFlag").style.display = "block";
  document.getElementById("startLights").style.display = "none";
  document.getElementById("winner").innerHTML = "Winner: " + drivers[winner] + " with " + maxLaps + " laps in " + calcLap(starttime, laptime);
  var cells = document.getElementById("topLine").children;
  cells[winner].style.backgroundColor = "lightgreen";
  raceActive = 2;
  document.getElementById("start").innerHTML = "Reset";
  document.getElementById("start").style.backgroundColor = "#00878F";
  document.getElementById("export").style.display = "block";
};

function resetScreen() {
  raceActive = 0;
  document.getElementById("start").innerHTML = "Start";
  var cells = document.getElementById("topLine").children;
  for (i = 1; i < 5;i++) {
    cells[i].style.backgroundColor = "white";
  };
  while (document.getElementById("lapTimes").rows.length > 1) {
    document.getElementById("lapTimes").deleteRow(1);
  };
  for (i = 1; i < 6; i++) {
    var myElememt = "d" + i;
    document.getElementById(myElememt).style.backgroundColor = "#bbb";
    document.getElementById(myElememt + "a").style.backgroundColor = "#bbb";
  };
  document.getElementById("finishFlag").style.display = "none";
  document.getElementById("liveTrack").style.display = "none";
  document.getElementById("startLights").style.display = "block";
  document.getElementById("export").style.display = "none";
  document.getElementById("jumpStart").style.display = "none";
  raceStartLights = 0;
  currentLap = 0;
  fastestLap = 4294967295;
};

function saveSettings() {
var needsave;
  needsave = false;
  for (i = 1; i < 5; i++) {
    if (document.getElementById("driver" + i).value != drivers[i]) {
      drivers[i] = document.getElementById("driver" + i).value;
      connection.send("W" + i + ":" + drivers[i]);
      needsave = true;
    };
  };
  if (document.getElementById("ssid").value != ssid) {
    ssid = document.getElementById("ssid").value;
    connection.send("W5:" + ssid);
    needsave = true;
  };
  if (document.getElementById("pwd").value != pwd) {
    pwd = document.getElementById("pwd").value;
    connection.send("W6:" + pwd);
    needsave = true;
  };
  maxLaps = document.getElementById("maxLaps").value;
  if (needsave) {
    connection.send('W8');
    console.log('send W8');
  };
  setupApp();
};

function fnExcelReport()
{
    var tab_text = '<table border="1px" style="font-size:20px" ">';
    var textRange;
    var j = 0;
    var tab = document.getElementById('lapTimes'); // id of table
    var lines = tab.rows.length;

    // the first headline of the table
    if (lines > 0) {
        tab_text = tab_text + '<tr bgcolor="#DFDFDF">' + tab.rows[0].innerHTML + '</tr>';
    }

    // table data lines, loop starting from 1
    for (j = 1 ; j < lines; j++) {
        tab_text = tab_text + "<tr>" + tab.rows[j].innerHTML + "</tr>";
    }

    tab_text = tab_text + "</table>";
    tab_text = tab_text.replace(/<A[^>]*>|<\/A>/g, "");             //remove if u want links in your table
    tab_text = tab_text.replace(/<img[^>]*>/gi,"");                 // remove if u want images in your table
    tab_text = tab_text.replace(/<input[^>]*>|<\/input>/gi, "");    // reomves input params
    // console.log(tab_text); // aktivate so see the result (press F12 in browser)

    var ua = window.navigator.userAgent;
    var msie = ua.indexOf("MSIE ");

     // if Internet Explorer
    if (msie > 0 || !!navigator.userAgent.match(/Trident.*rv\:11\./)) {
        txtArea1.document.open("txt/html","replace");
        txtArea1.document.write(tab_text);
        txtArea1.document.close();
        txtArea1.focus();
        sa = txtArea1.document.execCommand("SaveAs", true, "Durans_Speedway.xls");
    }
    else // other browser not tested on IE 11
        sa = window.open('data:application/vnd.ms-excel,' + encodeURIComponent(tab_text));

    return (sa);
} ;
