const char MAIN_page[] PROGMEM = R"=====(
<!doctype html>
<html>

<head>
  <title>Line Chart | Circuits4you.com</title>
  <!--For offline ESP graphs see this tutorial https://circuits4you.com/2018/03/10/esp8266-jquery-and-ajax-web-server/ -->
  <script src = "https://cdnjs.cloudflare.com/ajax/libs/Chart.js/2.7.3/Chart.min.js"></script>  
  <style>
  canvas{
    -moz-user-select: none;
    -webkit-user-select: none;
    -ms-user-select: none;
  }

  /* Data Table Styling */
  #dataTable {
    font-family: "Trebuchet MS", Arial, Helvetica, sans-serif;
    border-collapse: collapse;
    width: 100%;
  }

  #dataTable td, #dataTable th {
    border: 1px solid #ddd;
    padding: 8px;
  }

  #dataTable tr:nth-child(even){background-color: #f2f2f2;}

  #dataTable tr:hover {background-color: #ddd;}

  #dataTable th {
    padding-top: 12px;
    padding-bottom: 12px;
    text-align: left;
    background-color: #4CAF50;
    color: white;
  }
  </style>
</head>

<body>
    <div style="text-align:center;"><b>Circuits4you.com</b><br>Real Time Data Humidity, Temperature Logging with Graphs on ESP8266</div>
    <div class="chart-container" position: relative; height:350px; width:100%">
        <canvas id="Chart" width="400" height="400"></canvas>
    </div>
<div>
  <table id="dataTable">
    <tr><th>Time</th><th>ADC Value</th><th>Temperaure (&deg;C)</th><th>Humidity (%)</th></tr>
  </table>
</div>
<br>
<br>  

<script>
//Graphs visit: https://www.chartjs.org
var ADCvalues = [];
var Tvalues = [];
var Hvalues = [];
var timeStamp = [];
function showGraph()
{
    var ctx = document.getElementById("Chart").getContext('2d');
    var Chart2 = new Chart(ctx, {
        type: 'line',
        data: {
            labels: timeStamp,  //Bottom Labeling
            datasets: [{
                label: "Voltage 1",
                fill: false,  //Try with true
                backgroundColor: 'rgba( 243,18, 156 , 1)', //Dot marker color
                borderColor: 'rgba( 243, 18, 156 , 1)', //Graph Line Color
                data: ADCvalues,
            },{
                label: "Temperature",
                fill: false,  //Try with true
                backgroundColor: 'rgba( 243, 156, 18 , 1)', //Dot marker color
                borderColor: 'rgba( 243, 156, 18 , 1)', //Graph Line Color
                data: Tvalues,
            },
            {
                label: "Humidity",
                fill: false,  //Try with true
                backgroundColor: 'rgba(156, 18, 243 , 1)', //Dot marker color
                borderColor: 'rgba(156, 18, 243 , 1)', //Graph Line Color
                data: Hvalues,
            }],
        },
        options: {
            title: {
                    display: true,
                    text: "ADC Voltage"
                },
            maintainAspectRatio: false,
            elements: {
            line: {
                    tension: 0.5 //Smoothening (Curved) of data lines
                }
            },
            scales: {
                    yAxes: [{
                        ticks: {
                            beginAtZero:true
                        }
                    }]
            }
        }
    });

}

//On Page load show graphs
window.onload = function() {
  console.log(new Date().toLocaleTimeString());
};

//Ajax script to get ADC voltage at every 5 Seconds 
//Read This tutorial https://circuits4you.com/2018/02/04/esp8266-ajax-update-part-of-web-page-without-refreshing/

setInterval(function() {
  // Call a function repetatively with 5 Second interval
  getData();
}, 5000); //5000mSeconds update rate
 
function getData() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
     //Push the data in array
  var time = new Date().toLocaleTimeString();
  var txt = this.responseText;
  var obj = JSON.parse(txt); //Ref: https://www.w3schools.com/js/js_json_parse.asp
      ADCvalues.push(obj.ADC);
      Tvalues.push(obj.Temperature);
      Hvalues.push(obj.Humidity);
      timeStamp.push(time);
      showGraph();  //Update Graphs
  //Update Data Table
    var table = document.getElementById("dataTable");
    var row = table.insertRow(1); //Add after headings
    var cell1 = row.insertCell(0);
    var cell2 = row.insertCell(1);
    var cell3 = row.insertCell(2);
    var cell4 = row.insertCell(3);
    cell1.innerHTML = time;
    cell2.innerHTML = obj.ADC;
    cell3.innerHTML = obj.Temperature;
    cell4.innerHTML = obj.Humidity;
    }
  };
  xhttp.open("GET", "readADC", true); //Handle readADC server on ESP8266
  xhttp.send();
}
    
</script>
</body>

</html>

)=====";