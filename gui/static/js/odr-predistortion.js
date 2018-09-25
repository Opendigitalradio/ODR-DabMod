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
            $('#capturelength').text(data.length);
            $('#tx_median').text(data.tx_median);
            $('#rx_median').text(data.rx_median);
        });
    });

    $('#dpdstatusbutton').click(function() {
        doApiRequestGET("/api/dpd_status", function(data) {
            console.log("dpd_status succeeded: " + JSON.stringify(data));
            $('#histogram').text(data.histogram);
        });
    });
});


// ToolTip init
$(function(){
    $('[data-toggle="tooltip"]').tooltip();
});
