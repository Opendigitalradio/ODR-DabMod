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
        setRc("gain", "digital", $('#digitalgain').val(),
            requestAllParams);
    });

    $('#setwindowlength').click(function() {
        setRc("guardinterval", "windowlen", $('#windowlength').val(),
            requestAllParams);
    });

    $('#setclip').click(function() {
        setRc("ofdm", "clip", $('#cfrclip').val(),
            requestAllParams);
        setRc("ofdm", "errorclip", $('#cfrerrorclip').val(),
            requestAllParams);
        setRc("ofdm", "cfr",
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
