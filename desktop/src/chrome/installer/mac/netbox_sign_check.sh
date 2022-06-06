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

main() {
    local dmg_file_path
    dmg_file_path="${1}"

    echo "${dmg_file_path}"
    if ! [[ -f "${dmg_file_path}" ]]; then
        echo "patch_dmg must exist and be a regualar file"
        return 1
    fi

    echo "mktemp -d /tmp/netbox_dmg.XXXXXXXXXX"
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

    echo "spctl --assess -vvvvvv ${dmg_app_dir}"
    spctl --assess -vvvvvv "${dmg_app_dir}"

    echo "codesign -vvvvvv ${dmg_app_dir}"
    codesign -vvvvvv "${dmg_app_dir}"

    echo "unmount_dmg ${dmg_dir}"
    unmount_dmg "${dmg_dir}"
    return 0
}

main "${@}"
exit ${?}
