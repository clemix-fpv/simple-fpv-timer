# SPDX-License-Identifier: GPL-3.0+

Import('env')

import os
import gzip
import shutil
from pathlib import Path, PurePosixPath
import glob
import subprocess
import json

def prepare_www_files(source, target, env):

    filetypes_to_gzip = ['js', 'html', 'css', 'map']
    ignore_suffix = []

    proj_dir = Path(env.get("PROJECT_DIR"))
    data_src_dir = os.path.join(proj_dir, 'data_src')
    tmp_dir = os.path.join(proj_dir, 'data_tmp')
    src_dir = os.path.join(proj_dir, 'src')
    dst_header_file = os.path.join(src_dir, "static_files.h")

    if(os.path.exists(tmp_dir)):
        print('  Delete temproary dir {}'.format(tmp_dir))
        shutil.rmtree(tmp_dir)

    print('  Re-creating empty temporary dir {} '.format(tmp_dir))
    os.mkdir(tmp_dir)

    files_to_gzip = []
    for extension in filetypes_to_gzip:
        files_to_gzip.extend(glob.glob(os.path.join(data_src_dir, '*.' + extension)))

    print('  files to gzip: ' + str(files_to_gzip))

    all_files = glob.glob(os.path.join(data_src_dir, '*.*'))
    files_to_copy = list(set(all_files) - set(files_to_gzip))


    print('  files to copy: ' + str(files_to_copy))
    dst_files = []
    for file in files_to_copy:
        if PurePosixPath(file).suffix in ignore_suffix:
            continue

        print('  COPY: ' + file)
        dst = os.path.join(tmp_dir, os.path.basename(file))
        shutil.copy(file, dst)
        dst_files.append(dst)


    for file in files_to_gzip:
        dst = os.path.join(tmp_dir, os.path.basename(file) + '.gz')
        dst_files.append(dst)
        print('  GZIP: ' + file + ' -> ' + dst)
        cmd = 'gzip -9 < {S} > {D}'.format(S=file, D=dst)
        subprocess.check_call(cmd, shell=True)

    fcnt = 0;
    if os.path.exists(dst_header_file):
        print('  Delete existing destination: ' + dst_header_file)
        os.remove(dst_header_file)
    with open(dst_header_file, 'a') as fdst:
        print('  CREATE: ' + dst_header_file)
        fdst.write('#include <stdio.h>\n')

        h_file_content = []
        for file in dst_files:
            cnt = 0
            filename = os.path.basename(file)
            with open(file, 'rb') as fsrc:
                print('   PROCESS: ' + filename)
                fdst.write("/* %s */\nconst unsigned char file_%02d[] = {" % (filename, fcnt))
                while 1:
                    byte = fsrc.read(1)
                    if not byte:
                        break
                    if cnt > 0:
                        fdst.write(", ")
                    if cnt % 8 == 0:
                        fdst.write("\n    ")
                    cnt+=1
                    fdst.write("0x%02x" % ord(byte))
                fdst.write("\n};\n\n")

            gzip = 0
            filetype ="text/html"
            if filename.endswith('.gz'):
                filename = filename[0:-3]
                gzip = 1
            if filename.endswith('.css'):
                filetype = "text/css"
            if filename.endswith('.js'):
                filetype = "text/javascript"
            if filename.endswith('.ogg'):
                filetype = "audio/ogg"
            if filename.endswith('.svg'):
                filetype = "image/svg+xml"
            h_file_content.append((filename, gzip, filetype, "file_%02d" %fcnt, cnt))
            fcnt+=1

        fdst.write("""
struct static_files {
    const char *name;
    unsigned int is_gzip;
    const char *type;
    const unsigned char *data;
    size_t data_len;
};

const struct static_files STATIC_FILES[] = {
""")

        for hc in h_file_content:
            fdst.write('    {.name = %-30s .is_gzip = %d, .type = %-18s .data = %-7s, .data_len = %-5d },\n' %('"/' + hc[0]+'",', hc[1], '"'+hc[2]+'",', hc[3], hc[4]))
        fdst.write("    {.name = NULL, .data = NULL}\n};\n")

    # Cleanup
    if(os.path.exists(tmp_dir)):
        print('  Delete temproary dir {}'.format(tmp_dir))
        shutil.rmtree(tmp_dir)


def load_default_config(source, target, env):
    proj_dir = Path(env.get("PROJECT_DIR"))
    config_file = os.path.join(proj_dir, 'config.json')
    dst_file = os.path.join(proj_dir, 'src', 'config_default.c')
    default_cfg_json = {}
    if os.path.isfile(config_file):
        with open(config_file, 'r') as f:
            default_cfg_json = json.load(f)


    with open(dst_file, 'w') as fdst:
        fdst.write("#include <config_default.h>\n\n\n")
        fdst.write("void cfg_default_set(config_data_t *cfg){\n")
        for key, value in default_cfg_json.items():
            fdst.write('  cfg_data_set_param(cfg, "{}", "{}");\n'.format(key, value))
        fdst.write("}")


proj_dir = env.get("PROJECT_DIR")
env.AddCustomTarget(
    name="js_app",
    dependencies=None,
    actions=["cd {}/js && esbuild src/app.ts --bundle --outfile=../data_src/app.js --minify --target=esnext --sourcemap".format(proj_dir)],
    title="esbuild src/app.ts",
    description="esbuild src/app.ts",
    always_build=True,
)

env.AddCustomTarget(
    name="static_files_h",
    dependencies=["js_app"],
    actions=prepare_www_files,
    title="Generate Header",
    description="Generates a header file static_files.h",
    always_build=True,
)

env.AddCustomTarget(
    name="default_config_c",
    dependencies=["js_app"],
    actions=load_default_config,
    title="Create default config",
    description="Generates config_default.c",
    always_build=True,
)

# forward option --upload-port
up_port = env.get('UPLOAD_PORT')
if up_port is None:
    up_port = ""
else:
    up_port = "--upload-port {}".format(up_port)


env.AddCustomTarget(
    name="update_fw",
    dependencies=["default_config_c"],
    actions=["pio run -t upload {}".format(up_port)],
    title="Update firmware",
    description="Do what ever is needed and update the ESP32",
    always_build=False,
)

env.AddCustomTarget(
    name="js_server",
    dependencies=None,
    actions=["cd {}/js && esbuild src/ctrld.ts --bundle --outfile=../server/www/ctrld.js --minify --target=esnext --sourcemap".format(proj_dir)],
    title="esbuild src/ctrld.ts",
    description="esbuild src/ctrld.ts",
    always_build=True,
)
