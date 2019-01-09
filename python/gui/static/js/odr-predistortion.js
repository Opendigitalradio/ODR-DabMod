//   Copyright (C) 2019
//   Matthias P. Braendli, matthias.braendli@mpb.li
//
//    http://www.opendigitalradio.org
//
//   This file is part of ODR-DabMod.
//
//   ODR-DabMod is free software: you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as
//   published by the Free Software Foundation, either version 3 of the
//   License, or (at your option) any later version.
//
//   ODR-DabMod is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with ODR-DabMod.  If not, see <http://www.gnu.org/licenses/>.

var adapt_dumps = [];

function resultrefresh() {
    var jqxhr = doApiRequestGET("/api/dpd_results", function(data) {
        var summary = "";
        console.log(data);
        for (k in data['summary']) {
            summary += data['summary'][k];
            summary += "<br />";
        }
        $('#dpdresults').html(summary);

        $('#dpdstatus').text(data['state']);
        var percentage = data['stateprogress'];
        if (percentage > 100) {
            percentage = 100;
        }
        $('#dpdprogress').css('width', percentage + '%');
        $('#dpdprogresstext').text(percentage + '%');

        if (data['statplot']) {
            $('#dpdcapturestats').attr('src', data['statplot']);
        }
        else {
            $('#dpdcapturestats').attr('src', "");
        }

        $('#dpdmodeldata').html(data['modeldata']);

        if (data['modelplot']) {
            $('#dpdmodelplot').attr('src', data['modelplot']);
        }
        else {
            $('#dpdmodelplot').attr('src', "");
        }

        adapt_dumps = data['adapt_dumps'];
    });

    jqxhr.always(function() {
        setTimeout(resultrefresh, 2000);
    });
}

function adaptdumpsrefresh() {
    $('#dpdadaptdumps').html("");

    $.each(adapt_dumps, function(i, item) {
        console.log(item);

        if (isNaN(+item)) {
            $('#dpdadaptdumps').append($('<option>', {
                value: item,
                text : "DPD settings from " + item,
            }));
        }
        else {
            var d = new Date(0);
            d.setUTCSeconds(item);

            $('#dpdadaptdumps').append($('<option>', {
                value: item,
                text : "DPD settings from " + d.toISOString(),
            }));
        }
    });
}

$(function(){
    setTimeout(resultrefresh, 20);

    $('#calibratebtn').click(function() {
        doApiRequestPOST("/api/dpd_calibrate", {}, function(data) {
            console.log("calibrate succeeded: " + JSON.stringify(data));
        });
    });

    $('#triggerbtn').click(function() {
        doApiRequestPOST("/api/dpd_trigger_run", {}, function(data) {
            console.log("run succeeded: " + JSON.stringify(data));
        });
    });

    $('#adaptbtn').click(function() {
        doApiRequestPOST("/api/dpd_adapt", {}, function(data) {
            console.log("adapt succeeded: " + JSON.stringify(data));
        });
    });


    $('#resetbtn').click(function() {
        doApiRequestPOST("/api/dpd_reset", {}, function(data) {
            console.log("reset succeeded: " + JSON.stringify(data));
        });
    });

    $('#adaptdumpsrefreshbtn').click(adaptdumpsrefresh);

    $('#adaptdumpsload').click(function() {
        var elt = document.getElementById("dpdadaptdumps");

        if (elt.selectedIndex != -1) {
            var selectedoption = elt.options[elt.selectedIndex].value;
            doApiRequestPOST("/api/dpd_restore_dump", {dump_id: selectedoption}, function(data) {
                console.log("reset succeeded: " + JSON.stringify(data));
            });
        }
    });
});


// ToolTip init
$(function(){
    $('[data-toggle="tooltip"]').tooltip();
});
