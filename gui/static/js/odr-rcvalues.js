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

function buttonSetRc(key, controllable, param) {
    var value = $("#" + key).val();

    setRc(controllable, param, value, function(data) {
        requestStatus();
    });
}

function requestStatus() {
    $('#rctable > tbody').empty();

    doApiRequestGET("/api/rc_parameters", function(data) {
        $.each( data, function( key1, controllable ) {
            $.each( controllable, function( key2, param ) {
                var key = key1 + "_" + key2;
                var valueentry = '<input type="text" id="input'+key+'" ' +
                    'value="' + param['value'] + '">' +
                    '<button type="button" class="btn btn-xs btn-warning"' +
                    'id="button'+key+'" >upd</button>';

                $('#rctable > tbody:last').append(
                    '<tr><td>'+key+'</td>'+
                    '<td>'+valueentry+'</td>'+
                    '<td>'+param['help']+'</td></tr>');

                $('#button'+key).click(function() {
                    buttonSetRc("input"+key, key1, key2);
                });
            });
        });
    });
}


$(function(){
    requestStatus();

    $('#refresh').click(function() {
        requestStatus();
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
