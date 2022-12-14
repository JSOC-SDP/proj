var currentPage = 0;
function buttonForwardPress()
//this function executes when the next button is pressed and it loads the next image.
{
    var oldPage = null;
    if (currentPage < filenames.length - 1)
    {
        oldPage = currentPage;
        currentPage = currentPage + 1;
        $("#dropdown").val(currentPage.toString());
        $("#dropdown").selectmenu("refresh");
        hidePage(oldPage);
        showCurrentImage();
    }
}
function buttonBackwardPress()
//this function executes when the back button is pressed and loads the previous img
{
    var oldPage = null;
    if (currentPage > 0)
    {
        oldPage = currentPage;
        currentPage = currentPage - 1;
        $("#dropdown").val(currentPage.toString());
        $("#dropdown").selectmenu("refresh");
        hidePage(oldPage);
        showCurrentImage();
    }
}

var filenames = [];
var carrot = null;
var chMap = null;
var synopMap = null;
var wr = null;
var sr = null;

function fxn(data)
//this function takes the data from the manifest that was created in the python program and uses it to find the images inside of interal storgae 
{
    var pullDownNumber = 0;
    var heights = [];
    for (var image in data["slogfilenames"])
    {
        var file = data["slogfilenames"][image];
        filenames.push(file);
        heights.push(image);
    }
    chMap = data["chfilename"]
    synopMap = data["synopfilename"]
    wr = data["webroot"];
    heights.sort();
    for (var x in heights)
    {
        $("#dropdown").append("<option value=" + "'" + x + "' " + "id=" + "'" +"t"+ x + "'" + "></option>");
        $("#t" + pullDownNumber).html(heights[x]);
        pullDownNumber++;
    }
    filenames.sort();
    loadAllImages();
    showCurrentImage();
    carrot = data["carrot"];
    $("#carrotInput").val(carrot);
    day = data['date'];
    carRot();
    getDay();
    uiSetup();   
}

function hidePage(pageNo)
//hides the page that was previously displayed 
{
    var iframeID = "iframe" + pageNo.toString();
    $('#' + iframeID).hide();
}

function showCurrentImage()
//shows the next image when the next or previous button are hit
{
   var iframeID = "iframe" + currentPage.toString();
   var loaded = null;
   $("#" + iframeID).show();
}
function carRot()
//puts the carrington rotation at the top of the page
{
    $("#carrington").html("Carrington Rotation " + carrot);
}

function getDay()
//puts the day at the top of the page 
{
    $("#date").html(day);
}

function onMenuChange(event, ui)
//tells the program what to do when one of the dropdown selections are pressed 
{
    var oldPage = currentPage;
    currentPage = parseInt($("#dropdown").val());
    hidePage(oldPage);
    showCurrentImage();
}

function mapSwitch(event, ui)
//hides everything but the image that you have chosen to display 
{
    $("#mapswitchholder").show();
    if ($("#switch").val() == "chmap")
    {
        $("#qmapholder").hide();
        $("#synopFrame").hide();
        $("#chmapFrame").show();
    }
    if ($("#switch").val() == "synop") 
    {
        $("#qmapholder").hide();
        $("#chmapFrame").hide();
        $("#synopFrame").show();
    }
    if ($("#switch").val() == "slogq")
    {
        $("#synopFrame").hide();
        $("#chmapFrame").hide();
        $("#qmapholder").show();
        
    }
}

function loadAllImages()
//loads all of the images once the webpage is started becuase otherwise switching between images reloads them
{
     $("#mapswitchholder").append("<iframe id=" + '"chmapFrame"' + "class=" + '"Iframe"' +"></iframe>");
     $("#chmapFrame").attr("src", chMap);
     
     $("#mapswitchholder").append("<iframe id=" + '"synopFrame"' + "class=" + '"Iframe"' +"></iframe>");
     $("#synopFrame").attr("src", synopMap);
     
    for (var i in filenames)
    {
        var iframeID = "iframe" + i.toString();
        $("#qmap").append("<iframe id=" + '"' + iframeID + '"' + "class=" + '"Iframe"' +"></iframe>");
        $('#' + iframeID).attr("src", filenames[i]);
        $('#' + iframeID).hide();
    }
    $("#synopFrame").hide();
    $("#chmapFrame").hide();
}

function uiSetup()
//tells the dropdown menus what to do when they are pressed and gives them certain helpful characteristics 
{
    $("#switch").selectmenu();
    $("#switch").selectmenu({width: 185});
    $("#switch").selectmenu({ change: function(event,ui) {mapSwitch();}});
    $("#switch").selectmenu( "option", "position",{my: "left top",at: "left bottom",collision: "fit"});

    $("#dropdown").selectmenu();
    $("#dropdown").selectmenu({ change: function(event,ui) {onMenuChange();}});
    $("#dropdown").selectmenu({width: 100});
    $("#dropdown").selectmenu( "option", "position",{my: "left top",at: "left bottom",collision: "fit"});
}

function carrotSub()
{
    sr = $("#seriesInput").val();
    cr = parseInt($("#carrotInput").val());
    fr = $("#force").prop("checked");
    if (fr == true)
    {
        fr = 1;
    }

    else
    {
        fr = 0;
    }
    $("#carrotInput").prop("disabled", true);
    $("#seriesInput").prop("disabled", true);
    $("#submit").prop("disabled", true);
    $("#loader").show();
    $.ajax(
    {
        url: "http://jsoc.stanford.edu/cgi-bin/qmapviewer",
        data: {'carrot': cr, 'webroot': "/web/jsoc/htdocs/ajax/qmaps", 'series': sr, 'force': fr},
        type: 'POST',
        success: function(response)
        {
            if (response.status == 0)
            {
                location.reload();
            }
            else
            {
                $("#loader").hide();
                alert(response.errmsg);
                $("#carrotInput").prop("disabled", false);
                $("#seriesInput").prop("disabled", false);
                $("#submit").prop("disabled", false);
            }
        },
        error: function(error) 
        {
            alert("ERROR")
        }
    });
}