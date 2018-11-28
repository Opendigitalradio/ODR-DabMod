//   Copyright (C) 2018
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

$(function(){
    $('#capturebutton').click(function() {
        doApiRequestPOST("/api/trigger_capture", {}, function(data) {
            console.log("trigger_capture succeeded: " + JSON.stringify(data));
        });
    });

    $('#dpdstatusbutton').click(function() {
        doApiRequestGET("/api/dpd_status", function(data) {
            console.log("dpd_status succeeded: " + JSON.stringify(data));
            $('#histogram').text(data.histogram);
            $('#capturestatus').text(data.capture.status);
            $('#capturelength').text(data.capture.length);
            $('#tx_median').text(data.capture.tx_median);
            $('#rx_median').text(data.capture.rx_median);
        });

    $.ajax({
        type: "GET",
        url: "/api/dpd_capture_pointcloud",

        error: function(data) {
            if (data.status == 500) {
                var errorWindow = window.open("", "_self");
                errorWindow.document.write(data.responseText);
            }
            else {
                $.gritter.add({ title: 'API',
                    text: "AJAX failed: " + data.statusText,
                    image: '/fonts/warning.png',
                    sticky: true,
                });
            }
        },
        success: function(data) {
            $('#dpd_pointcloud').value(data)
        }
    })
    });
});


// ToolTip init
$(function(){
    $('[data-toggle="tooltip"]').tooltip();
});
