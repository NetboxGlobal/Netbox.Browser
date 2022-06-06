#!/bin/bash -p

mount_dmg() {
    local dmg_file_path="${1}"
    local dmg_dir="${2}"

    echo "hdiutil attach ${dmg_file_path} -mountpoint ${dmg_dir} -nobrowse -owners off"
    hdiutil attach "${dmg_file_path}" -mountpoint "${dmg_dir}" -nobrowse -owners off > /dev/null
    return ${?}
}

unmount_dmg() {
    local dmg_dir="${1}"

    echo "hdiutil detach ${dmg_dir} -force >/dev/null"
    hdiutil detach "${dmg_dir}" -force >/dev/null

    echo "rm -rf ${dmg_dir}"
    rm -rf "${dmg_dir}"
}

do_update() {
    local current_dir="${1}"
    local current_version="${2}"
    local new_dir="${3}"

    # update executable first
    if [ -x "${new_dir}/Contents/MacOS/NetboxBrowser" ]; then
        # create backup of old executable file
        cur_executable="${current_dir}/Contents/MacOS/NetboxBrowser"
        tmp_executable="${current_dir}/Contents/MacOS/NetboxBrowser.tmp"

        echo "cp ${cur_executable} ${tmp_executable}"
        cp "${cur_executable}" "${tmp_executable}"

        rm -f "${cur_executable}"

        echo cp "${new_dir}/Contents/MacOS/NetboxBrowser ${cur_executable}"
        if cp "${new_dir}/Contents/MacOS/NetboxBrowser" "${cur_executable}"; then
            echo "rm -f ${tmp_executable}"
            rm -f "${tmp_executable}"
        else
            # rollback
            echo "mv ${tmp_executable} ${cur_executable}"
            mv "${tmp_executable}" "${cur_executable}"
            return 1
        fi
    fi

    # copy new files
    echo "ditto ${new_dir}/Contents ${current_dir}/Contents"
    if ! ditto "${new_dir}/Contents" "${current_dir}/Contents"; then
        echo "failed to copy new version"
        return 1;
    fi

    # update resources
    echo "cp -R ${new_dir}/Contents/Resources ${current_dir}/Contents/Resources_new"
    if cp -R "${new_dir}/Contents/Resources" "${current_dir}/Contents/Resources_new"; then
        mv "${current_dir}/Contents/Resources" "${current_dir}/Contents/Resources_old"
        mv "${current_dir}/Contents/Resources_new" "${current_dir}/Contents/Resources"
        rm -rf "${current_dir}/Contents/Resources_old"
    fi

    # remove old version
    local old_folder="${current_dir}/Contents/Frameworks/NetboxBrowser Framework.framework/Versions/${current_version}"
    echo "rm -rf ${old_folder}"
    rm -rf "${old_folder}"

    # remove attr
    echo xattr -d -r com.apple.quarantine "${current_dir}"
    xattr -d -r com.apple.quarantine "${current_dir}"
}

main() {
    local current_dir="${1}"
    local current_version="${2}"
    local dmg_file_path="${3}"

    if ! [[ -d "${current_dir}" ]]; then
        echo "current_dir must exist and be a directory"
        return 1
    fi

    if ! [[ -f "${dmg_file_path}" ]]; then
        echo "patch_dmg must exist and be a regualar file"
        return 1
    fi

    dmg_dir=$(mktemp -d /tmp/netbox_dmg.XXXXXXXXXX)
    if ! mount_dmg "${dmg_file_path}" "${dmg_dir}"; then
        echo "failed to mount ${dmg_file_path} to ${dmg_dir}"
        rm -rf "${dmg_dir}"
        return 1
    fi

    local dmg_app_dir="${dmg_dir}/NetboxBrowser.app"
    if [[ ! -d ${dmg_app_dir} ]]; then
        echo "failed to get ${dmg_app_dir}"
        unmount_dmg "${dmg_dir}"
        return 1
    fi

    do_update "$current_dir" "$current_version" "$dmg_app_dir"
    local return_code=${?}

    unmount_dmg "${dmg_dir}"
    return $return_code
}

# create log file
is_logging="${6}"
if [[ -n ${is_logging} ]]; then
    log_file="/tmp/netbox_updater.log"
    touch $log_file
    exec 1>$log_file
    exec 2>&1
fi

echo "cd /tmp"
cd /tmp || exit
echo "pwd"
pwd

# stop wallet
echo "wallet_pid=${5}"
wallet_pid="${5}"
if [[ ${wallet_pid} != "0" ]]; then
    echo "kill -s SIGTERM ${wallet_pid}"
    kill -s SIGTERM "${wallet_pid}"
fi

# wait for parent
echo "lsof -p ${4} +r 1 &>/dev/null"
lsof -p "${4}" +r 1 &>/dev/null

# wait for wallet
echo "lsof -p ${5} +r 1 &>/dev/null"
lsof -p "${5}" +r 1 &>/dev/null

# do update
main "${@}"

# launch browser after update
echo "${1}/Contents/MacOS/NetboxBrowser"
"${1}"/Contents/MacOS/NetboxBrowser