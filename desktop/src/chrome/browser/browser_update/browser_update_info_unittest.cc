#include "chrome/browser/browser_update/browser_update_info.h"

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/browser_update/browser_update.h"
#include "chrome/common/chrome_version.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(NetboxUnit_BrowserUpdateInfo, ConstructUrl) 
{        
    EXPECT_EQ(BrowserUpdateInfo::get_update_url(), 
              std::string("http://cdn.netbox.global/update.json"));
}


bool should_we_do_update(std::string test)
{
    BrowserUpdateInfo b;
    BrowserUpdate::FileMetadata meta = b.get_file_metadata_from_json(test);

    return !meta.update_url.empty() && !meta.update_hash.empty();
}


TEST(NetboxUnit_BrowserUpdateInfo, get_file_metadata_from_json) 
{        
    // not json
    EXPECT_EQ(false, should_we_do_update("blbla {}"));

    // incorrect json    
    EXPECT_EQ(false, should_we_do_update("{\"version\":\"1.0.0.0\"}"));    

    // empty checksum
    EXPECT_EQ(false, should_we_do_update("{\"version\":\"172.0.3626.53\",\"url\":\"http://cdn.netbox.global/172.0.3626.53/browser.exe\",\"hash\":\"5c2041ae034f6a0baaaba561c3cb75894e0fa16a54fb234c07631e5e5531fdda\",\"walleturl\":\"http://cdn.netbox.global/netboxglobal/wallet.exe\",\"wallethash\":\"6099d02767e520283c634326e6d8a59cd85a139f6085c740e379a77d68a7cb84\",\"checksum\":\"\"}"));

    // incorrect checksum
    EXPECT_EQ(false, should_we_do_update("{\"version\":\"172.0.3626.53\",\"url\":\"http://cdn.netbox.global/172.0.3626.53/browser.exe\",\"hash\":\"5c2041ae034f6a0baaaba561c3cb75894e0fa16a54fb234c07631e5e5531fdda\",\"walleturl\":\"http://cdn.netbox.global/netboxglobal/wallet.exe\",\"wallethash\":\"6099d02767e520283c634326e6d8a59cd85a139f6085c740e379a77d68a7cb84\",\"checksum\":\"123456\"}"));

    // read "out/default/test.update.json" and check it
    std::string contents;    

    base::FilePath path;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &path);
    path = path.AppendASCII("out");
    path = path.AppendASCII("default");
    path = path.AppendASCII("test.update.json");
    EXPECT_EQ(true, ReadFileToString(path, &contents));
    EXPECT_EQ(true, should_we_do_update(contents));
}


