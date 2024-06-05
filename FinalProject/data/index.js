function navContent() {
    var x = document.getElementById("navMenu");
    if(x.style.display === "block"){
        x.style.display = "none";
    } else {
        x.style.display = "block";
    }
}

const download = document.getElementById("downloadBtn");
const clearDate = document.getElementById("clearCsv");
const clearWifi = document.getElementById("clearConfig");

download.addEventListener("click", function(){
    downloadCsv();
});

clearDate.addEventListener("click", function(){
    deleteData();
});
clearWifi.addEventListener("click", function(){
    clearConfig();
});

var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
var chart;
var dates = [];

window.addEventListener('load', onLoad);

function onLoad(event) {
  initWebSocket();
  initChart(); // Initialize the chart when the page loads
}

function initWebSocket() {
    websocket = new WebSocket(gateway);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
}

function onOpen(event) {
    console.log('Connection opened');
    getData(); // Initial data fetch
}

function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
}

function onMessage(event) {
    console.log('Message received:', event.data);
    appendDataToChart(event.data);
}

function getData(){
    fetch("/getdata").then(response => response.text())
    .then(csvData => {
        updateChart(csvData);
    });
}

function initChart() {
    chart = Highcharts.chart('chart-temperature', {
        title: {
            text: 'Temperature over Time'
        },
        xAxis: {
            categories: [],
            title: {
                text: 'Date'
            }
        },
        yAxis: {
            title: {
                text: 'Temperature (°C)'
            }   
        },
        series: [{
            type: 'line',
            name: 'Temperature',
            data: []
        }]
    });
}

function updateChart(csvData) {
    // Split the CSV data into rows
    const rows = csvData.trim().split('\n');
    // Extract the data rows
    const dataRows = rows.slice(1).map(row => {
        const values = row.split(',');
        return { date: values[0], temperature: parseFloat(values[1]) }; // Convert temperature to a number
    });

    dates = dataRows.map(row => row.date);
    const temperatures = dataRows.map(row => row.temperature);

    // Update the chart with new data
    chart.xAxis[0].setCategories(dates);
    chart.series[0].setData(temperatures);
}

function appendDataToChart(csvRow) {
    const values = csvRow.split(',');
    const date = values[0];
    const temperature = parseFloat(values[1]);

    // Add the new point to the chart
    dates.push(date);
    chart.xAxis[0].setCategories(dates);
    chart.series[0].addPoint(temperature, true, false);
}

function deleteData(){
    fetch("/delete").then(response => response.text())
    .then(csvData => {
        alert(csvData);
        window.location.reload();
    }).
    catch(error => console.error(error));
}

function downloadCsv() {
    fetch("/download")
        .then(response => response.text())
        .then(data => {
            const blob = new Blob([data], { type: 'text/csv' });
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = 'data.csv';
            document.body.appendChild(a);
            a.click();
            document.body.removeChild(a);
        });
    console.log("Downloaded");
}


function clearConfig(){
    fetch("/clearconfig").then(response => response.text())
    .then(data => {
        alert(data);
        window.location.reload();
    }).
    catch(error => console.error(error));
}