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

function set_rc(controllable, param, value, callback) {
    $.ajax({
        type: "POST",
        url: "/api/parameter",
        contentType: 'application/json',
        dataType: 'json',
        data: JSON.stringify({
            controllable: controllable,
            param: param,
            value: value
        }),

        error: function(data) {
            $.gritter.add({
                title: 'RC set',
                text: "ERROR",
                image: '/fonts/warning.png',
                sticky: true,
            });
        },
        success: callback
    });
}

function getRc(callback) {
    $.ajax({
        type: "GET",
        url: "/api/rc_parameters",
        contentType: 'application/json',
        dataType: 'json',

        error: function(data) {
            $.gritter.add({
                title: 'RC info',
                text: "ERROR: ",
                image: '/fonts/warning.png',
                sticky: true,
            });
        },
        success: callback
    });
}

function requestAllParams(callback) {
    getRc(function(data) {
        $('#windowlength').val(data.guardinterval.windowlen.value);
        $('#digitalgain').val(data.gain.digital.value);
        $('#cfrenable').prop("checked", data.ofdm.cfr.value == 1);
        $('#cfrclip').val(data.ofdm.clip.value);
        $('#cfrerrorclip').val(data.ofdm.errorclip.value);
        $('#cfrstats').val(data.ofdm.clip_stats.value);
        $('#paprstats').val(data.ofdm.papr.value);
    });
}

function requestStats(callback) {
    getRc(function(data) {
        $('#cfrstats').val(data.ofdm.clip_stats.value);
        $('#paprstats').val(data.ofdm.papr.value);
    });
}

var updateTimer = setInterval(requestStats, 2000);

$(function(){
    requestAllParams();

    $('#setdigitalgain').click(function() {
        set_rc("gain", "digital", $('#digitalgain').val(),
            requestAllParams);
    });

    $('#setwindowlength').click(function() {
        set_rc("guardinterval", "windowlen", $('#windowlength').val(),
            requestAllParams);
    });

    $('#setclip').click(function() {
        set_rc("ofdm", "clip", $('#cfrclip').val(),
            requestAllParams);
        set_rc("ofdm", "errorclip", $('#cfrerrorclip').val(),
            requestAllParams);
        set_rc("ofdm", "cfr",
            ($('#cfrenable').prop("checked") ? "1" : "0"),
            requestAllParams);
    });

    $('#refresh').click(function() {
        requestAllParams();
        $.gritter.add({
                    title: 'Services status refresh',
                    image: '/fonts/accept.png',
                    text: 'Ok',
                });
    });
});


// ToolTip init
$(function(){
    $('[data-toggle="tooltip"]').tooltip();
});
