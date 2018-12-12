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
        console.log(data);
        let controllable_names = Object.keys(data);
        controllable_names.sort();

        var key1;
        for (key1 in controllable_names) {
            let param_names = Object.keys(data[controllable_names[key1]]);
            param_names.sort();

            var key2;
            for (key2 in param_names) {
                var name_controllable = controllable_names[key1];
                var name_param = param_names[key2];
                var key = name_controllable + "_" + name_param;

                var param = data[name_controllable][name_param];
                var valueentry = '<input type="text" id="input'+key+'" ' +
                    'value="' + param['value'] + '">' +
                    '<button type="button" class="btn btn-xs btn-warning"' +
                    'id="button'+key+'" ' +
                    'data-controllable="'+name_controllable+'" ' +
                    'data-param="'+name_param+'" ' +
                    '>upd</button>';

                $('#rctable > tbody:last').append(
                    '<tr><td>'+key+'</td>'+
                    '<td>'+valueentry+'</td>'+
                    '<td>'+param['help']+'</td></tr>');

                $('#button'+key).click(function() {
                    var attr_c = this.getAttribute('data-controllable');
                    var attr_p = this.getAttribute('data-param');
                    var k = attr_c + "_" + attr_p;
                    buttonSetRc("input"+k, attr_c, attr_p);
                });
            }
        }
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
