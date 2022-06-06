/** @const {string} Path to root from chrome/test/data/webui/settings/. */
// eslint-disable-next-line no-var
var ROOT_PATH = '../../../../../';

// Polymer BrowserTest fixture.
GEN_INCLUDE([ROOT_PATH + 'chrome/test/data/webui/polymer_browser_test_base.js']);

/**
 * @constructor
 * @extends {PolymerTest}
 */
function WalletPageBrowserTest() {}

WalletPageBrowserTest.prototype = {
  __proto__: PolymerTest.prototype,

  /** @override */
  browsePreload: 'chrome://wallet/',

  /** @type {?SettingsBasicPageElement} */
  basicPage: null,

  extraLibraries: PolymerTest.getLibraries(ROOT_PATH),
};

function WalletUIBrowserTest() {}

WalletUIBrowserTest.prototype = {
  __proto__: WalletPageBrowserTest.prototype,

};

TEST_F('WalletUIBrowserTest', 'All', function() {
  suite('wallet-ui', function() {
    test('basic', function() {
        assertEquals('foo', 'foo');
    });

    test('basic2', function() {
        assertEquals('foo', 'bar');
    });
  });

  mocha.run();
});
