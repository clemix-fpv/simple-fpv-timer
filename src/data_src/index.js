// SPDX-License-Identifier: GPL-3.0+

var tab_name = "laps";
const gateway = `ws://${window.location.hostname}:${window.location.port}/ws/rssi`;
var websocket;
const ctx =  {
    config: {},
    time_offset: 0,
};

function init_infosocket() {
    var gateway = `ws://${window.location.hostname}:${window.location.port}/ws/info`;
    console.log('Trying to open a WebSocket connection...' + gateway);
    websocket = new WebSocket(gateway);
    websocket.onopen    = onInfoOpen;
    websocket.onclose   = onInfoClose;
    websocket.onmessage = onInfoMessage; // <-- add this line
}

function onInfoOpen(event) {
}

function onInfoClose(event) {
    setTimeout(init_infosocket, 1000);
}

function onInfoMessage(event) {
    try {
        json = JSON.parse(event.data);
    } catch (e) {
        return console.error(e); // error in the above string (in this case, yes)!
    }
}



function init_websocket() {
    console.log('Trying to open a WebSocket connection...' + gateway);
    websocket = new WebSocket(gateway);
    websocket.onopen    = onOpen;
    websocket.onclose   = onClose;
    websocket.onmessage = onMessage; // <-- add this line
}

function send_data() {
    const msg =  {
        type: "HELLO"
    };
    websocket.send(JSON.stringify(msg));
}

function onOpen(event) {
    on_rssi_update({"CONNECTION": "OPEN"});
    setTimeout(send_data, 2000);

}

function onClose(event) {
    on_rssi_update({"CONNECTION": "CLOSED"});
    if (tab_name == "debug"){
        setTimeout(init_websocket, 2000);
    }
}

function onMessage(event) {
    try {
        json = JSON.parse(event.data);
        if (json.type == "rssi") {
            on_rssi_update(json.data);
            spectrum_update(json);
            for (const e of json.data) {
                graph_update(e);
            }
        }

    } catch (e) {
        console.error(event.data);
        return console.error(e); // error in the above string (in this case, yes)!
    }
}

function on_rssi_update(j)
{
    let dbg = $('#debug');
    $('#dbg_rssi_card').remove();
    t = $('<table>')
    j = j[j.length - 1];
    for (let key in j){
        let value = j[key];
        t.append(`<tr><td>${key}</td><td>${value}</td></tr>`);
    }
    dbg.prepend(build_card('RSSI:', t, "dbg_rssi_card"));

    if (tab_name != "debug"){
        websocket.close();
    }
}

function simple_api_call(cmd)
{
    $.post('/api/v1/'+cmd, {},
        function (ret_data){
        }
    );
}

function format_ms(ms)
{
    var sec_num = parseInt(ms, 10);
    var hours   = Math.floor(sec_num / 3600 / 1000);
    var minutes = Math.floor((sec_num - (hours * 3600 * 1000)) / 60 / 1000);
    var seconds = Math.floor((sec_num - (hours * 3600 * 1000)  - (minutes * 60 * 1000)) / 1000);
    var ms = sec_num - (hours * 3600 * 1000) - (minutes * 60 * 1000) - seconds * 1000;

    var t = [[hours, "h"], [minutes,"m"], [seconds,"s"], [ms,"ms"]];
    var p = false;
    var ret = "";
    for (i=0; i < t.length; i++){
        if (t[i][0] > 0 || p) {
            ret += t[i][0] + t[i][1] + " ";
            p=true;
        }
    }
    return ret;
}

function notify(success, msg)
{
    var e = $('#box_notify');
    if (success) {
        e.addClass("border-success bg-success");
        e.removeClass("border-danger bg-danger");
    } else {
        e.addClass("border-danger bg-danger");
        e.removeClass("border-success bg-success");
    }
    e.find('h5').html(msg);
    e.stop();
    e.css('top', 0);
    e.animate({top: -1 * e.height() - 5}, 1500);
}

function notify_response(msg, data)
{
    j = data.responseJSON ? data.responseJSON : data;
    if (j.status == "ok")
    {
        msg += " SUCCESSFUL";
        if (j.msg)
            msg += " - " + j.msg;
        notify(true, msg)
    } else {
        msg += " FAILED";
        if (j.msg)
            msg += " - " + j.msg;
        notify(false, msg);
    }
}

function build_freq(desc, cfg_value)
{
    name = desc.name;
    var h = '<div class="form-group" id="fg_settings_'+name+'">';
    h += '<fieldset>';
    h += '<label class="form-label">Frequenz ('+cfg_value+'MHz)</label><br/>';

    var channels =  [
        { freq: 5658, name: "R1 (5658)"  },
        { freq: 5695, name: "R2 (5695)"  },
        { freq: 5732, name: "R3 (5732)"  },
        { freq: 5769, name: "R4 (5769)"  },
        { freq: 5806, name: "R5 (5806)"  },
        { freq: 5843, name: "R6 (5843)"  },
        { freq: 5880, name: "R7 (5880)"  },
        { freq: 5917, name: "R8 (5917)"  },
    ];
    h += '<select name="'+name+'" class="custom-select custom-select-lg mb-3">';
    $.each(channels, function(i, o){
        var selected = (cfg_value == o.freq) ? "selected" : "";
        h += '<option value="'+o.freq+'" '+selected+' >'+o.name+'</option>';
    });
    h += '</select>';
    h += '</fieldset>';
    h += '</div>';

    return h;
}

function build_options(desc, cfg_value)
{
    var name = desc.name;
    var label = desc.label;
    var options = desc.options;

    var h = '<div class="form-group" id="fg_settings_'+name+'">';
    h += '<fieldset>';
    h += '<label class="form-label">'+label+'</label><br/>';

    var channels =  [

    ];
    h += '<select name="'+name+'" class="custom-select custom-select-lg mb-3">';
    $.each(options, function(i, o){
        var selected = (cfg_value == o.value) ? "selected" : "";
        h += '<option value="'+o.value+'" '+selected+' >'+o.name+'</option>';
    });
    h += '</select>';
    h += '</fieldset>';
    h += '</div>';

    return h;
}

function build_wifi_mode(key, value)
{
    var h = '<div class="form-group" id="fg_settings_'+key+'">';
    h += '<fieldset>';
    h += '<label class="form-label">Wifi Mode</label><br/>';

    var channels =  [
        { freq: 0, name: "Access Point"  },
        { freq: 1, name: "Station"  },
    ];
    h += '<select name="'+key+'" class="custom-select custom-select-lg mb-3">';
    $.each(channels, function(i, o){
        var selected = (value == o.freq) ? "selected" : "";
        h += '<option value="'+o.freq+'" '+selected+' >'+o.name+'</option>';
    });
    h += '</select>';
    h += '</fieldset>';
    h += '</div>';

    return h;
}


function build_common_setting(o, value)
{
    var key = o.name;
    var label = o.label ? o.label : key;
    var s='<div class="form-group" id="fg_settings_'+key+'">' +
        '<fieldset>'+
        '<label class="form-label" for="input_'+key+'">'+label;
    if (o.help && 0) {
        s += '<a style="margin: 0 5px" href="#" data-toggle="tooltip" data-trigger="click" title="'+o.help+'">'+
            '<img src="/question-circle.svg"/>'
            '</a>';
    }
    s+= '</label>'+
        '<input class="form-control input_'+key+'" name="'+key+'" id="input_'+key+'" type="text" value="'+value+'">'+
        '</fieldset>'+
        '</div>';
    return $(s);
}

function on_btn_test_osd(e)
{
    e.preventDefault();
    data = {};

    var form = $(e.target).parents("form");
    data.x = form.find('.input_osd_x').val();
    data.y = form.find('.input_osd_y').val();
    var text_elem = form.find('.input_osd_text');
    data.text = text_elem.val();
    if (data.text.length == 0)
        data.text = text_elem.attr('placeholder');
    var format_elem = form.find('.input_osd_format');
    if (format_elem.length) {
        data.format = format_elem.val();
        if (data.format.length == 0)
            data.format = format_elem.attr('placeholder');
    }
    data.method = $(e.target).data('method');
    $.ajax({
        url: '/api/v1/osd/' + data.method,
        type: 'POST',
        contentType: "application/json",
        data: JSON.stringify(data),
    }).always(function(data){
        notify_response("Send OSD message", data);
    });
}

function build_osd_test(debug)
{
    var row = $('<div class="form-row mb-2">');
    var col = $('<div class="col-auto">');
    row.append(col);
    col.append('<label class="form-label" for="osd_test_text">Test osd message:</label>');
    col.append('<input type="text" class="form-control mb-2 input_osd_text" placeholder=" 2: 2.32 (- 0.44)">');

    col = $('<div class="col-auto">');
    row.append(col);
    col.append('<span id="test_osd_msg" style="opasity: 0">');
    if (debug) {
        $.each(['clear', 'display','set_text'], function(index, value) {
            var btn = $('<a href="" class="btn btn-secondary btn-sm active" style="float:right;" data-method="'+value+'" role="button">'+value+'</a>');
            btn.on("click",on_btn_test_osd);
            col.append(btn);
        });
    }
    var btn = $('<a href="#" class="btn btn-secondary btn-sm active" style="float:right;" data-method="display_text" role="button" aria-pressed="true" tooltip="Check you goggles to see the OSD message">Test</a>');
    btn.click(on_btn_test_osd);
    col.append(btn);
    btn = $('<a href="#" class="btn btn-secondary btn-sm active" style="float:right;" data-method="test_format" role="button" aria-pressed="true" tooltip="Check current format string">Check Format</a>');
    btn.click(on_btn_test_osd);
    col.append(btn);
    return row;
}


function build_osd_format(key, value)
{
    var row = $('<div class="mb-3">');
    row.append('<label class="form-label" for="input_osd_format">OSD format string</label>');
    row.append('<input name="osd_format" type="text" id="input_osd_format" class="form-control input_osd_format" placeholder="%2L: %5.2ts(%6.2ds)" value="'+value+'">');
    row.append('<div class="form-text" id="basic-addon4">%L=lap number, %tm=time in minutes, %ts=time in seconds, %tms=time in milliseconds, %dm=difference to fastes lap in minutes, %ds=difference to fastes lap in seconds, %dms=difference to fastes lap in milliseconds</div>');
    return row;
}

function build_common_status(key, value)
{
    return  '<tr><td>'+key+'</td><td>'+value+'</td></tr>';
}

function build_common_status_players(key, players)
{
    var h = "";
    $.each ( players, function (i, p) {
        for(var key in p) {
            if (key == "laps" ) {
                h += '<table>';
                h += '<thead><tr><td>id</td><td>duration</td><td>rssi</td></tr></thead>';
                h += '<tbody>';
                $.each ( p.laps, function (i, v) {
                    h += '<tr><td>'+ v.id +'</td><td>'+ v.duration +'</td><td>'+v.rssi+'</td></tr>';
                });
                h += '</tbody>';
                h += '</table>';
            } else {
                h += "<div>" + key + ": " + p[key] + "</div>";
            }
        }
    });
    return build_common_status(key, h);
}

function objectifyForm(formArray) {
    //serialize data function
    var returnArray = {};
    for (var i = 0; i < formArray.length; i++){
        returnArray[formArray[i]['name']] = formArray[i]['value'];
    }
    return returnArray;
}

function build_led_color(desc, cfg_val)
{
    var color = cfg_val.toString(16).padStart(6, '0');
    var h = '<label for="exampleColorInput" class="form-label">'+ desc.label +'</label>';
    h += '<input type="color" class="form-control form-control-color" value="#'+color+'" title="Choose your color" name="'+desc.name+'">';
    return h;
}

function build_settings(config)
{
    var s = $('#settings');
    s.empty();

    var form_groups = [
        {
            name: "Player settings",
            elements: [
                {
                    name: "rssi[0].freq",
                    label: "Frequency",
                    draw_fn: build_options,
                    options: [
                        { value: 5658, name: "R1 (5658)"  },
                        { value: 5695, name: "R2 (5695)"  },
                        { value: 5732, name: "R3 (5732)"  },
                        { value: 5769, name: "R4 (5769)"  },
                        { value: 5806, name: "R5 (5806)"  },
                        { value: 5843, name: "R6 (5843)"  },
                        { value: 5880, name: "R7 (5880)"  },
                        { value: 5917, name: "R8 (5917)"  },
                    ],
                },
                {
                    name: "rssi[0].name",
                    label: "Player Name",
                },
                {
                    name: "rssi[0].led_color",
                    label: "LED color",
                    draw_fn: build_led_color,
                }
            ]
        },
        {
            name: "Callibration/Meassurment",
            elements: [
                {
                    label: "RSSI Peak value (default: 1100)",
                    name: "rssi[0].peak",
                    help: "The expected RSSI maximum, when the drone fly through the gate."
                },
                {
                    name: "rssi[0].filter",
                    label: "Filter",
                    help: "Smooth the RSSI input signals. Range is 1-100, a value near" +
                          " to 1 smooth more, a value of 100 keeps the raw RSSI value."
                },
                {
                    label: "Offset enter (in %)",
                    name: "rssi[0].offset_enter",
                    help: "The percentage of 'RSSI-Peak' to count a drone as entered" +
                          " the gate. The range is 50-100 (Default: 80)"
                },
                {
                    label: "Offset leave (in %)",
                    name: "rssi[0].offset_leave",
                    help: "The percentage of 'RSSI-Peak' to count a drone as leaved the"+
                          " gate. The range is 50-100 (Default: 70)"
                },
                {
                    label: "Calibration max lap count",
                    name: "rssi[0].calib_max_lap_count",
                    help: "The number of laps to be detected to finish the calibration."
                },
                {
                    label: "Calibration min rssi peak",
                    name: "rssi[0].calib_min_rssi_peak",
                    help: "The minimum RSSI value to detect a 'drone enter gate' during" +
                          " calibration."
                }
            ],
        },
        {
            name: "expressLRS/OSD",
            elements: ["elrs_uid", "osd_x", "osd_y", "osd_format", "osd_test"]
        },
        {
            name: "WiFi",
            elements: ["wifi_mode", "ssid", "passphrase"]
        },
        {
            name: "General",
            elements: [
                {
                    name: "game_mode",
                    label: "Game Mode",
                    draw_fn: build_options,
                    options: [
                        {value: 0, name: 'RACE'},
                        {value: 1, name: 'CAPTURE THE FLAG'},
                        {value: 2, name: 'SPECTROMETER'},
                    ]
                }
            ]
        }
    ];

    form = $('<form class="form-horizontal" action="" id="form_settings" method="post"/>');
    var lg = $('<ul class="list-group">');
    for(var fg of form_groups){
        var lgi = $('<li class="list-group-item bg-gradient">');
        lgi.append('<h5 style="text-align: right" >'+fg.name+'</h5>');
        for(var elem of fg.elements) {
            var h = "";
            var o = typeof elem == 'object' ? elem : {name: elem};
            var name = o.name;
            if (o.draw_fn) {
                h = o.draw_fn(o, config[name], config);
            } else if (name == "osd_test") {
                h = build_osd_test();
            } else if (name == "osd_format") {
                h = build_osd_format(name, config[name]);
            } else if (name == "wifi_mode") {
                h = build_wifi_mode(name, config[name]);
            } else {
                h = build_common_setting(o, config[name]);
            }
            lgi.append(h);
        }
        lg.append(lgi);
    }

    form.append(lg);
    form.append('<div class="d-flex justify-content-center"><button type="submit" class="btn btn-primary">Submit</button></div>');
    s.append(form);
    form.on( "submit", function( event ) {
        event.preventDefault();
        let mydata = objectifyForm($(this).serializeArray());
        $.ajax({
            url: '/api/v1/settings',
            type: 'POST',
            contentType: "application/json; charset=utf-8",
            dataType: "json",
            data: JSON.stringify(mydata)
        }).always(function(data){
            notify_response("Configuration saved", data);
        });

    });

    //$('a[data-toggle="tooltip"]').click(function(e) {
    //    e.tooltip("toggle");
    //    return false;
    //});

    $('a[data-toggle="tooltip"]').tooltip({
        animated: 'fade',
        placement: 'top',
        trigger: 'click'
    });

    //$('a[data-toggle="tooltip"]').on('show.bs.tooltip', function (e) {
    //    $('a[data-toggle="tooltip"]').each( function () {
    //        $(this).tooltip("hide");
    //    });
    //});
}

function update_settings()
{
    $.ajax({
        url: '/api/v1/settings'
    })
        .done(function(data) {
            ctx.config = data.config;
            build_settings(data.config);
        })
        .fail(function() {
            notify_response("Failed to get settings", xhr);
        });
}

var round_idx = {};
var sound;
function on_new_lap(lap)
{
    if($('.btn_enable_sound').data('sound-enabled')){
        sound.play();
    }
}

function update_laps()
{
    $.ajax({
        url: '/api/v1/settings'
    })
        .done(function(data) {
            var stats = data.status;

            if (stats.in_calib_mode[0]) {
                $('#b_cali').hide();
                $('#p_cali').show();
                var p_bar = $('div#p_cali > div');
                var w = Math.floor(stats.in_calib_lap_count[0] / data.config.calib_max_lap_count[0] * 100);
                w = w < 10 ? 10: w;
                p_bar.css("width", w + "%");
                p_bar.attr('aria-valuemin', 0);
                p_bar.attr('aria-valuemax', data.config.calib_max_lap_count[0]);
                p_bar.attr('aria-valuenow', stats.in_calib_lap_count[0]);
                p_bar.text(stats.in_calib_lap_count[0] + "/" + data.config.calib_max_lap_count[0] + "laps");
            } else {
                $('#b_cali').show();
                $('#p_cali').hide();
            }
            var tb = $('#d_labs tbody');
            tb.empty();
            var rows = [];
            $.each(stats.players, function(i,p){

                var fastes = 99999999999999;
                $.each(p.laps, function (i, v) {
                    var t = v.duration;
                    if (t == 0) return;
                    if (fastes > t)
                        fastes = t;
                });
                if (p.laps.length > 0) {
                    var last_id = p.laps[p.laps.length - 1].id;
                    if (!(p.name in round_idx) || round_idx[p.name] != last_id) {
                        on_new_lap(p.laps[p.laps.length - 1])
                        round_idx[p.name] = last_id;
                    }
                }

                $.each(p.laps, function (i, v) {
                    if (v.duration == 0) return;
                    v.fastes = v.duration == fastes;
                    v.player = p.name;
                    rows.push(v);
                });
            });
            rows.sort(function(a,b){
                return a.abs_time < b.abs_time;
            });

            $.each(rows, function (i, v) {
                var c = 'table-light';
                if (v.fastes) {
                    c = 'table-success';
                }
                var h = '<tr class="' + c + '">';
                h += '<td>' + v.player + '</td>';
                h += '<td>' + v.id + '</td>';
                h += '<td>' + format_ms(v.duration) + '</td>';
                h += '<td>' + v.rssi + '</td>';
                h += '</tr>';
                tb.append($(h));
            });

        });

    if (tab_name == 'laps') {
        setTimeout(update_laps, 2000);
    }
}

function update_players()
{
    $.ajax({
        url: '/api/v1/settings'
    })
        .done(function(data) {
            var stats = data.status;
            if (!stats) {
                console.error("Failed to get stats!");
                console.debug(data);
                return;
            }

            var cards = [];
            $.each(stats.players, function(i,p){

                var fastes = null;
                var time_sum = 0;
                $.each(p.laps, function (i, v) {
                    if (!fastes){
                        fastes = v;
                    } else if(fastes.duration > v.duration) {
                        fastes = v;
                    }
                    time_sum += v.duration;
                });

                if (p.laps.length > 0) {
                    var last_id = p.laps[p.laps.length - 1].id;
                    if (!(p.name in round_idx) || round_idx[p.name] != last_id) {
                        on_new_lap(p.laps[p.laps.length - 1])
                        round_idx[p.name] = last_id;
                    }
                }

                cards.push({
                    name: p.name,
                    ipaddr: p.ipaddr,
                    time: time_sum > 0 ? time_sum : 0,
                    lap: p.laps.length > 0 ? p.laps[p.laps.length-1].id : 0,
                    fastes: fastes
                });
            });

            cards.sort(function(a,b){
                var lap_diff = b.lap - a.lap;
                if (lap_diff != 0)
                    return lap_diff;

                t_a = a.time > 0 ? a.time : Number.MAX_SAFE_INTEGER;
                t_b = b.time > 0 ? b.time : Number.MAX_SAFE_INTEGER;

                return t_a == t_b ? 0 : (t_a > t_b)? 1 : -1;
            });

            $.each(cards, function (i, v) {
                v.rank = i + 1;
            });

            cards.sort(function(a,b){
                return a.name.localeCompare(b.name);
            });

            $('#player_cards').empty();
            $.each(cards, function (i, v) {
                card = $('<div class="card" style="margin: 5px;">');
                var medal = "";
                medal = (v.rank == 1)? "&#127942;"/*"&#129351;"*/ :
                    (v.rank == 2)? "&#129352;" :
                    (v.rank == 3)? "&#129353;" :
                    v.rank + ".";
                var btn_edit = (v.ipaddr)? '<a href="http://'+v.ipaddr+'" class="btn btn-secondary btn-sm" style="float:right" role="button" >&#9881;</a>':'';
                card.append('<h5 class="card-header">'+ medal +" "+ v.name + btn_edit +'</h5>');
                table = $('<table>');
                table.append('<tr><td class="card-text" >Lap:</td><td class="card-text">'+v.lap+'</td></tr>');
                table.append('<tr><td class="card-text" >Time:</td><td class="card-text">'+ (v.time > 0 ? format_ms(v.time) : 0) +'</td></tr>');
                fastes = "";
                if (v.fastes) {
                    fastes = format_ms(v.fastes.duration) + ' <small class="text-muted">(Lap ' + v.fastes.id + ')</small>';
                }
                table.append('<tr><td class="card-text">Fastes:</td><td class="card-text">'+ fastes +'</td></tr>');
                card.append(table);
                $('#player_cards').append(card);
            });
        });

    if (tab_name == 'players') {
        setTimeout(update_players, 1000);
    }
}

function build_card(title, content, id)
{
    card = $('<div class="card border-primary mb-3" style="max-width: 20rem;">')
    if (typeof id !== 'undefined'){
        card.attr('id', id);
    }
    card_header = $('<div class="card-header">');
    card_header.html(title);
    card_body = $('<div class="card-body">');
    card_body.html(content);
    card.append(card_header);
    card.append(card_body);
    return card;
}

function format_int(i, f) {
    if (!f) {
        f = 0;
    }
    i = parseInt(i, 10);

    return (i).toLocaleString(
      undefined,
      { minimumFractionDigits: f, maximumFractionDigits: f}
    );
}

function format_ms_short(ms) {
    var sec_num = parseInt(ms, 10);
    var hours   = Math.floor(sec_num / 3600 / 1000);
    var minutes = Math.floor((sec_num - (hours * 3600 * 1000)) / 60 / 1000);
    var seconds = Math.floor((sec_num - (hours * 3600 * 1000)  - (minutes * 60 * 1000)) / 1000);
    var ms = sec_num - (hours * 3600 * 1000) - (minutes * 60 * 1000) - seconds * 1000;

    var t = [[hours, "h"], [minutes,"m"], [seconds,"."], [ms,"s"]];
    var ret = "";
    for (i=0; i < t.length; i++){
        ret += "" + t[i][0] + t[i][1];
    }
    return ret;
}

let uplot = null;
let time0 = 0;
function graph(id)
{
    let xs = [1,2,3,4,5,6,7,];
    let vals = [-10,-9,-8,-7];

    let data = [
        new Float32Array(xs),
        new Float32Array(xs.map((t, i) => vals[Math.floor(Math.random() * vals.length)])),
    ];

    data = [
        [],  /* time */
        [],  /* rssi_peak */
        [],  /* rssi_enter */
        [],  /* rssi_leave */
        [],  /* rssi */
        [],  /* rssi_smooth */
    ];

    let e = $('#' + id);
    const opts = {
        width: e.innerWidth(),
        height: 600,
        title: "RSSI",
        scales: {
            x: {
                time: false,
            },
            y: {
                auto:false,
                range: [100, 1200],
            }
        },
        axes: [
            {
              values: (self, ticks) => ticks.map(rawValue => format_ms_short(rawValue)),
            },
        ],
        series: [
            {
                label: "millis",
                value: (u, v) => v == null ? null : format_ms_short(v),
            },
            {
                stroke: "#70dba4",
                label: "peak",
            },
            {
                stroke: "#dbc270",
                label: "enter",
            },
            {
                stroke: "#3af51d",
                label: "leave",
            },
            {
                stroke: "red",
                label: "rssi",
            },
            {
                stroke: "green",
                label: "rssi-smooth",
            },
        ],
          };

    uplot = new uPlot(opts, data, document.getElementById(id));
}

let collect_rssi = [];
let laps = [];
let color = [255, 165, 0];
function graph_update(json)
{
    let data = uplot._data;
    let timeduration = 10000;
    console.debug("graph_update");

    /* the order of the data is important, as we would like to see the
     * rssi and rssi-smoothed in foreground */
    const idx_next_lap = 1 + laps.length;
    const idx_rssi_peak = 1 + laps.length;
    const idx_rssi_enter = 2 + laps.length;
    const idx_rssi_leave = 3 + laps.length;
    const idx_rssi = 4 + laps.length;
    const idx_rssi_smooth = 5 + laps.length;

    if (time0 == 0)
        time0 = json.t;

    t = json.t - time0;
    data[0].push(t);       /* time value */
    data[idx_rssi].push(json.r);
    data[idx_rssi_smooth].push(json.s);

    /* Remove old time data */
    let older_as = t - timeduration;
    while(data[0][0] < older_as){
        data[0].shift();
        data[idx_rssi].shift();
        data[idx_rssi_smooth].shift();
    }

    /* update horizontal lines for RSSI thresholds from configuration */
    let rssi_peak = ctx.config ? ctx.config['rssi[0].peak'] : 0;
    if (rssi_peak && data[0].length > 0) {
        let offset_enter = ctx.config['rssi[0].offset_enter'];
        let offset_leave = ctx.config['rssi[0].offset_leave'];
        const rssi_enter = Math.floor(rssi_peak * (offset_enter / 100));
        const rssi_leave = Math.floor(rssi_peak * (offset_leave / 100));
        len = data[0].length;
        if (data[idx_rssi_peak].length != len || data[idx_rssi_peak][0] != rssi_peak)
            data[idx_rssi_peak] = Array(len).fill(rssi_peak);
        if (data[idx_rssi_enter].length != len || data[idx_rssi_enter][0] != rssi_enter)
            data[idx_rssi_enter] = Array(len).fill(rssi_enter);
        if (data[idx_rssi_leave].length != len || data[idx_rssi_leave][0] != rssi_leave)
            data[idx_rssi_leave] = Array(len).fill(rssi_leave);
    }

    /* Update collected "drone in gate" events */
    if (json.i){
        collect_rssi.push(json)
    } else {
        if (collect_rssi.length > 10) {

            c = "rgba("+Math.floor(Math.random() * 255) + "," +
                        Math.floor(Math.random() * 255) + "," +
                        Math.floor(Math.random() * 255) + ", 0.1)"
            uplot.addSeries({
                stroke: c,
				fill: c,
                label: "Lap " + (laps.length + 1),
			}, idx_next_lap);
            data.splice(idx_next_lap, 0, [] );

            laps.push({idx: idx_next_lap, data: collect_rssi})
            collect_rssi = new Array();
        }
    }
    $.each(laps, function (i,o){
        let arr = data[o.idx];
        if (data[0].length == arr.length) return;

        let start_idx = Math.floor((data[0].length - o.data.length) / 2);

        for (let index = 0; index < start_idx; index++) {
            arr[index] = 0;
        }

        $.each(o.data, function(j,u){
            arr[j+start_idx] = u.r;
        });
    });

    uplot.setData(data);
}

let uplot_spectrum = null;
const spectrum_data = [];


function spectrum_update_data()
{
    var data = new Array();

    /* Build the x-Axes */
    var x = new Array();
    x = spectrum_data.map((x) => {
            return x.freq;
        });
    x.sort();

    let diff = 40;
    if (x.length > 1) {
        for (let i = 1; i < x.length; i++) {
            diff += x[i] - x[i-1];
        }
        diff /= x.length -1;
    }
    x.unshift(x[0] - diff);
    x.push(x[x.length-1] + diff);


    /* build the values for series 1 */
    s1 = new Array();
    s2 = new Array();
    s3 = new Array();

    var sec_1 = (Date.now()) - 1000;
    var sec_2 = (Date.now()) - 2000;
    var sec_3 = (Date.now()) - 3000;


    x.forEach((freq) => {
        e = spectrum_data.find(x => x.freq == freq);
        if (e) {
            let i = 0;
            for (; i < e.data.length; i++) {
                const element = e.data[i];
                if (element.time >= sec_1) {
                    s1.push(element.rssi);
                    break;
                }
            }
            if (i >= e.data.length) {
                s1.push(0);
                i=0;
            }

            for (; i < e.data.length; i++) {
                const element = e.data[i];
                if (element.time < sec_1 && element.time > sec_2) {
                    s2.push(element.rssi);
                    break;
                }
            }
            if (i >= e.data.length) {
                s2.push(0);
                i=0;
            }

            for (; i < e.data.length; i++) {
                const element = e.data[i];
                if (element.time < sec_2 && element.time > sec_3) {
                    s3.push(element.rssi);
                    break;
                }
            }
            if (i >= e.data.length) {
                s3.push(0);
            }
        } else {
            s1.push(0);
            s2.push(0);
            s3.push(0);
        }
    });

    ret = new Array();
    ret.push(x);
    ret.push(s3);
    ret.push(s2);
    ret.push(s1);
    return ret;
}

function spectrum(id)
{
    let e = $('#' + id);
    const opts = {
        height: 600,
        width: e.innerWidth(),
        title: "Spectrum",
        scales: {
            x: {
                time: false,
            },
            y: {
                auto:false,
                range: [100, 1200],
            }
        },
        series: [
            {
            },
            {
                paths: uPlot.paths.bars(),
                fill: "rgba(150,0,0,0.1)",
            },
            {
                paths: uPlot.paths.bars(),
                fill: "rgba(200,0,0,0.3)",
            },

            {
                paths: uPlot.paths.bars(),
                fill: "rgba(255,0,0,1.0)",
            },
        ],
    };


    uplot_spectrum = new uPlot(opts, [[],[],[],[]], document.getElementById(id));
    return uplot_spectrum;
}

var spectrum_refresh_timer = false;
function spectrum_refresh()
{
    if (!spectrum_refresh_timer) {
        spectrum_refresh_timer = true;
        setTimeout(spectrum_refresh_do, 500);
        return;
    }
}

function spectrum_refresh_do()
{
    var data = spectrum_update_data();
    if (uplot_spectrum) {
        uplot_spectrum.setData(data);
    }
    spectrum_refresh_timer = false;
}

function spectrum_update(ev)
{
    if (!ev || !ev.freq) {
        return;
    }
    e = spectrum_data.find(x => x.freq == ev.freq);
    if (!e) {
        e = {freq: ev.freq, data: []};
        spectrum_data.push(e);
    }

    for (const measurement of json.data) {
        e.data.unshift({
            time: measurement.t + ctx.time_offset,
            rssi: measurement.r,
        })
        e.data.splice(10,e.data.length - 10);
    }

    spectrum_refresh();
 }

function ival(obj)
{
    if (obj) {
        return obj.val()? obj.val(): obj.attr('placeholder');
    }
    return null;
}

function on_btn_lap_debugging(e)
{
    e.preventDefault();
    data = {};

    var form = $(e.target).parents("form");
    var method = $(e.target).data('method');

    data.player = ival(form.find('.input_player_name'));

    if (method == "lap") {
        data.lap = {
            id: ival(form.find('.input_lap_id')),
            rssi: ival(form.find('.input_lap_rssi')),
            duration: ival(form.find('.input_lap_duration')),
        };
    }

    $.ajax({
        url: '/api/v1/player/' + method,
        type: 'POST',
        contentType: "application/json",
        data: JSON.stringify(data),
    }).always(function(data){
        notify_response("Send lap debug message", data);
    });
}

function player_debugging()
{
    var row = $('<div class="form-row mb-2">');
    var form = $('<form>');
    row.append(form);
    var col = $('<div class="col-auto">');
    form.append(col);
    col.append('<label class="form-label" for="player_name">Connect new Player:</label>');
    col.append('<input type="text" class="form-control mb-2 input_player_name" placeholder="clemix">');
    col = $('<div class="col-auto">');
    form.append(col);

    var value = 'connect'
    var btn = $('<a href="" class="btn btn-secondary btn-sm active" style="" data-method="'+value+'" role="button">'+value+'</a>');
    btn.on("click",on_btn_lap_debugging);
    col.append(btn);

    row.append('<hr>')
    /* lap form */
    form = $('<form>');
    row.append(form);
    var col = $('<div class="col-auto">');
    form.append(col);
    col.append('<label class="form-label" for="player_name">Player:</label>');
    col.append('<input type="text" class="form-control mb-2 input_player_name" placeholder="clemix">');
    col.append('<label class="form-label" for="player_name">lap.id:</label>');
    col.append('<input type="text" class="form-control mb-2 input_lap_id" placeholder="99">');
    col.append('<label class="form-label" for="player_name">lap.rssi:</label>');
    col.append('<input type="text" class="form-control mb-2 input_lap_rssi" placeholder="1000">');
    col.append('<label class="form-label" for="player_name">lap.duration:</label>');
    col.append('<input type="text" class="form-control mb-2 input_lap_duration" placeholder="66">');
    col = $('<div class="col-auto">');
    form.append(col);

    var value = 'lap'
    var btn = $('<a href="" class="btn btn-secondary btn-sm active" style="float:right;" data-method="'+value+'" role="button">'+value+'</a>');
    btn.on("click",on_btn_lap_debugging);
    col.append(btn);

    return row;
}

function update_debug()
{
    $.ajax({
        url: '/api/v1/settings'
    })
        .done(function(data) {
            let dbg = $('#debug');
            dbg.empty();
            ctx.config = data.config;
            /* build status table */
            let status_table = $('<table class="table">');
            status_table.append('<thead><tr><th>name</th><th>value</th></tr></thead>');
            stats = data.status;
            for(let key in stats) {
                let h;
                if (key == 'players') {
                    h = build_common_status_players(key, stats[key]);
                } else {
                    h = build_common_status(key, stats[key]);
                }

                status_table.append(h);
            }

            osd_form = $('<form class="form-horizontal" action="" id="form_settings" method="post"/>');
            $.each(['osd_x', 'osd_y'], function(i,v){
                osd_form.append(build_common_setting(v, data.config[v]));
            });
            osd_form.append(build_osd_test(true));
            dbg.append(build_card('Player Debugging', player_debugging));

            dbg.append(build_card('OSD Debugging:', osd_form));
            dbg.append(build_card('Status:', status_table));

            dbg.append(build_card('Graph:', "<div id='id_graph'/>"));
            dbg.append(build_card('Graph:', "<div id='id_spectrum'/>"));
            graph("id_graph");
            spectrum("id_spectrum");
        });

    if (tab_name == 'debug') {
        //setTimeout(update_debug, 500);
    }
}

function sync_time_finish(data)
{
    var c0 = data.client[0];
    var c1 = data.client[1];
    var c2 = data.client[2];
    var c3 = data.client[3];
    var s0 = data.server[0];
    var s1 = data.server[1];
    var s2 = data.server[2];
    var s3 = data.server[3];

    var rtt1 = (c1-c0 + s1-s0) / 4;      // how long it takes to get the package to server
    var offset1 = c1 - (s1 - rtt1);

    var rtt2 = (c3-c2 + s3-s2) / 4;      // how long it takes to get the package to server
    var offset2 = c2 - (s2 - rtt2);

    var offset = (offset1 + offset2) / 2;
    return Math.round(offset);
}

function sync_time(data) {
    if (! data) {
        data = { 'client': [Date.now()]};
    }

    $.ajax({
        url: '/api/v1/time-sync',
        type: 'POST',
        contentType: "application/json",
        data: JSON.stringify(data),
    }).always(function(resp){
            if (resp.client && resp.server) {
                if (resp.client.length < 4) {
                    resp.client.push(Date.now());
                    sync_time(resp);
                } else {
                    ctx.time_offset = sync_time_finish(resp);
                }
            }
    });
}

function hide_all_tooltips()
{
    $('.tooltip').hide();
}

$('.nav a[href$=settings]').on('click', function(e){
    tab_name = "settings";
    hide_all_tooltips();
    update_settings();
});

$('.nav a[href$=players]').on('click', function(e){
    tab_name = "players";
    hide_all_tooltips();
    update_players();
});

$('.nav a[href$=laps]').on('click', function(e){
    tab_name = "laps";
    hide_all_tooltips();
    update_laps();
});

$('.nav a[href$=debug]').on('click', function(e){
    tab_name = "debug";
    hide_all_tooltips();
    update_debug();
    init_websocket();
});

$('.btn_enable_sound').click(function(){
    var enabled = ! $(this).data('sound-enabled');
    $('.btn_enable_sound').data('sound-enabled', enabled)
    $('.btn_enable_sound').html(enabled ? '&#x1F50A;' : '&#128263;');
    $('.btn_enable_sound').attr('title', "Disable sounds");
});

/* this is called on page load */
$(function () {
    update_laps();

    $('[data-toggle="tooltip"]').tooltip({trigger: 'hover'});
    Howler.autoUnlock = false;
    sound = new Howl({
        src: ['round.ogg']
    });

    sync_time();


});



