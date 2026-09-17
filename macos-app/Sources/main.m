#import <Cocoa/Cocoa.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

typedef void (^ToolCompletion)(int, NSString *, NSString *);

@interface AppDelegate : NSObject <NSApplicationDelegate>
@property NSWindow *window;
@property NSPopUpButton *devicePopup;
@property NSTextField *deviceLabel;
@property NSTextField *firmwareLabel;
@property NSTextField *statusLabel;
@property NSProgressIndicator *progress;
@property NSTextView *logView;
@property NSButton *refreshButton;
@property NSButton *probeButton;
@property NSButton *chooseButton;
@property NSButton *upgradeButton;
@property NSURL *firmwareURL;
@property NSArray<NSDictionary *> *devices;
@property BOOL busy;
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    [self buildWindow];
    [self refreshFirmwareLabel:nil];
    [self.window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
    [self refreshDevices:nil];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    return YES;
}

- (NSTextField *)label:(NSString *)text size:(CGFloat)size weight:(NSFontWeight)weight {
    NSTextField *label = [NSTextField labelWithString:text];
    label.font = [NSFont systemFontOfSize:size weight:weight];
    return label;
}

- (void)buildWindow {
    self.window = [[NSWindow alloc]
        initWithContentRect:NSMakeRect(0, 0, 760, 610)
        styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                  NSWindowStyleMaskMiniaturizable
        backing:NSBackingStoreBuffered
        defer:NO];
    self.window.title = @"Keychron Mouse Firmware Updater";
    self.window.releasedWhenClosed = NO;
    [self.window center];

    NSTextField *title = [self label:@"Keychron 鼠标固件升级工具" size:24
                              weight:NSFontWeightSemibold];
    NSTextField *subtitle = [NSTextField wrappingLabelWithString:
        @"先选择有线连接的 Keychron 鼠标，再选择与设备型号一致的签名固件。"
         "升级期间应用会阻止系统休眠，请勿拔线。"];
    subtitle.textColor = NSColor.secondaryLabelColor;

    NSTextField *step1 = [self label:@"1. 选择鼠标" size:15 weight:NSFontWeightSemibold];
    self.devicePopup = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
    self.devicePopup.target = self;
    self.devicePopup.action = @selector(deviceSelectionChanged:);
    [self.devicePopup addItemWithTitle:@"正在查找 Keychron 鼠标…"];
    [self.devicePopup.widthAnchor constraintGreaterThanOrEqualToConstant:420].active = YES;
    self.refreshButton = [NSButton buttonWithTitle:@"刷新设备" target:self
                                             action:@selector(refreshDevices:)];
    self.probeButton = [NSButton buttonWithTitle:@"重新读取" target:self
                                           action:@selector(probeDevice:)];
    NSStackView *deviceRow = [NSStackView stackViewWithViews:
        @[self.devicePopup, self.refreshButton, self.probeButton]];
    deviceRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    deviceRow.spacing = 10;

    self.deviceLabel = [self label:@"设备：等待检测" size:13 weight:NSFontWeightRegular];
    NSTextField *step2 = [self label:@"2. 选择固件" size:15 weight:NSFontWeightSemibold];
    self.chooseButton = [NSButton buttonWithTitle:@"选择 .signed.bin 固件…" target:self
                                           action:@selector(chooseFirmware:)];
    self.firmwareLabel = [NSTextField wrappingLabelWithString:@"固件：尚未选择"];
    self.firmwareLabel.textColor = NSColor.secondaryLabelColor;
    NSStackView *firmwareRow = [NSStackView stackViewWithViews:
        @[self.chooseButton, self.firmwareLabel]];
    firmwareRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    firmwareRow.spacing = 12;

    NSTextField *step3 = [self label:@"3. 校验并升级" size:15 weight:NSFontWeightSemibold];
    self.statusLabel = [NSTextField wrappingLabelWithString:
        @"鼠标必须切换到有线模式并使用 USB 线直连 Mac。"];
    self.statusLabel.textColor = NSColor.secondaryLabelColor;
    self.progress = [[NSProgressIndicator alloc] init];
    self.progress.indeterminate = NO;
    self.progress.minValue = 0;
    self.progress.maxValue = 100;
    self.upgradeButton = [NSButton buttonWithTitle:@"校验并开始升级" target:self
                                             action:@selector(beginUpgrade:)];
    self.upgradeButton.keyEquivalent = @"\r";

    self.logView = [[NSTextView alloc] init];
    self.logView.editable = NO;
    self.logView.selectable = YES;
    self.logView.font = [NSFont monospacedSystemFontOfSize:11 weight:NSFontWeightRegular];
    self.logView.textContainerInset = NSMakeSize(8, 8);
    NSScrollView *scroll = [[NSScrollView alloc] init];
    scroll.borderType = NSBezelBorder;
    scroll.hasVerticalScroller = YES;
    scroll.documentView = self.logView;
    scroll.translatesAutoresizingMaskIntoConstraints = NO;
    [scroll.heightAnchor constraintEqualToConstant:215].active = YES;

    NSStackView *stack = [NSStackView stackViewWithViews:
        @[title, subtitle, step1, deviceRow, self.deviceLabel, step2, firmwareRow,
          step3, self.statusLabel, self.progress, self.upgradeButton, scroll]];
    stack.orientation = NSUserInterfaceLayoutOrientationVertical;
    stack.alignment = NSLayoutAttributeLeading;
    stack.spacing = 10;
    stack.translatesAutoresizingMaskIntoConstraints = NO;
    self.window.contentView = [[NSView alloc] init];
    [self.window.contentView addSubview:stack];

    [NSLayoutConstraint activateConstraints:@[
        [stack.leadingAnchor constraintEqualToAnchor:self.window.contentView.leadingAnchor constant:28],
        [stack.trailingAnchor constraintEqualToAnchor:self.window.contentView.trailingAnchor constant:-28],
        [stack.topAnchor constraintEqualToAnchor:self.window.contentView.topAnchor constant:24],
        [stack.bottomAnchor constraintLessThanOrEqualToAnchor:self.window.contentView.bottomAnchor constant:-22],
        [subtitle.widthAnchor constraintEqualToAnchor:stack.widthAnchor],
        [self.deviceLabel.widthAnchor constraintEqualToAnchor:stack.widthAnchor],
        [self.firmwareLabel.widthAnchor constraintLessThanOrEqualToConstant:430],
        [self.statusLabel.widthAnchor constraintEqualToAnchor:stack.widthAnchor],
        [self.progress.widthAnchor constraintEqualToAnchor:stack.widthAnchor],
        [scroll.widthAnchor constraintEqualToAnchor:stack.widthAnchor],
    ]];
    [self updateControls];
}

- (NSDictionary *)selectedDevice {
    id represented = self.devicePopup.selectedItem.representedObject;
    return [represented isKindOfClass:NSDictionary.class] ? represented : nil;
}

- (NSString *)compatibilityLabelForDevice:(NSDictionary *)device {
    NSString *value = device[@"compatibility_status"];
    if ([value isEqual:@"hardware_verified"]) return @"已实机验证";
    if ([value isEqual:@"protocol_compatible_unverified_model"]) return @"协议兼容，待实机验证";
    if ([value isEqual:@"protocol_compatible_unverified_product_id"]) return @"协议兼容，该 PID 待实机验证";
    if ([value isEqual:@"probe_failed"]) return @"设备读取失败";
    if ([value isEqual:@"incompatible"]) return @"协议不兼容";
    return @"状态未知";
}

- (NSArray<NSString *> *)warningsForDevice:(NSDictionary *)device {
    id value = device[@"compatibility_warnings"];
    return [value isKindOfClass:NSArray.class] ? value : @[];
}

- (void)updateControls {
    NSDictionary *device = [self selectedDevice];
    BOOL usable = device && [device[@"compatible"] boolValue];
    self.devicePopup.enabled = !self.busy && self.devices.count > 0;
    self.refreshButton.enabled = !self.busy;
    self.probeButton.enabled = !self.busy && usable;
    self.chooseButton.enabled = !self.busy && usable;
    self.upgradeButton.enabled = !self.busy && usable && self.firmwareURL != nil;
}

- (void)refreshFirmwareLabel:(NSDictionary *)inspection {
    if (!self.firmwareURL) {
        self.firmwareLabel.stringValue = @"固件：尚未选择";
        return;
    }
    NSString *version = inspection[@"header"][@"version"];
    self.firmwareLabel.stringValue = version.length
        ? [NSString stringWithFormat:@"%@ · 版本 %@", self.firmwareURL.lastPathComponent, version]
        : self.firmwareURL.lastPathComponent;
}

- (void)setBusy:(BOOL)busy message:(NSString *)message {
    self.busy = busy;
    self.statusLabel.stringValue = message;
    [self updateControls];
}

- (void)appendLog:(NSString *)text {
    if (!text.length) return;
    NSString *cleaned = [text stringByReplacingOccurrencesOfString:@"\r" withString:@""];
    [self.logView.textStorage appendAttributedString:
        [[NSAttributedString alloc] initWithString:cleaned]];
    [self.logView scrollToEndOfDocument:nil];
    for (NSString *line in [cleaned componentsSeparatedByString:@"\n"]) {
        if (![line hasPrefix:@"Progress: "]) continue;
        NSString *rest = [line substringFromIndex:@"Progress: ".length];
        double percent = [[[rest componentsSeparatedByString:@"%"] firstObject] doubleValue];
        self.progress.doubleValue = percent;
        self.statusLabel.stringValue = [NSString stringWithFormat:@"正在写入固件：%.0f%%", percent];
    }
}

- (NSDictionary *)parseResult:(NSString *)output {
    NSData *data = [output dataUsingEncoding:NSUTF8StringEncoding];
    NSDictionary *root = data ? [NSJSONSerialization JSONObjectWithData:data options:0 error:nil] : nil;
    if (![root isKindOfClass:NSDictionary.class] || ![root[@"ok"] boolValue]) return nil;
    return [root[@"result"] isKindOfClass:NSDictionary.class] ? root[@"result"] : nil;
}

- (void)showError:(NSString *)title details:(NSString *)details {
    NSAlert *alert = [[NSAlert alloc] init];
    alert.alertStyle = NSAlertStyleCritical;
    alert.messageText = title;
    alert.informativeText = details.length ? details : @"请查看窗口中的日志。";
    [alert runModal];
}

- (void)runTool:(NSArray<NSString *> *)arguments busy:(NSString *)busyMessage
     completion:(ToolCompletion)completion {
    NSString *path = [[NSBundle mainBundle] pathForAuxiliaryExecutable:@"keychron-mouse-updater-cli"];
    if (!path) {
        [self showError:@"升级引擎缺失" details:@"请重新安装应用。"];
        return;
    }
    [self setBusy:YES message:busyMessage];
    [self appendLog:[NSString stringWithFormat:@"\n$ %@\n", [arguments componentsJoinedByString:@" "]]];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        NSTask *task = [[NSTask alloc] init];
        NSPipe *outPipe = [NSPipe pipe];
        NSPipe *errPipe = [NSPipe pipe];
        task.executableURL = [NSURL fileURLWithPath:path];
        task.arguments = arguments;
        task.standardOutput = outPipe;
        task.standardError = errPipe;
        NSError *launchError = nil;
        if (![task launchAndReturnError:&launchError]) {
            dispatch_async(dispatch_get_main_queue(), ^{
                [self setBusy:NO message:@"无法启动升级引擎"];
                [self showError:@"无法启动升级引擎" details:launchError.localizedDescription];
                completion(-1, @"", launchError.localizedDescription);
            });
            return;
        }
        NSData *outData = [outPipe.fileHandleForReading readDataToEndOfFile];
        NSData *errData = [errPipe.fileHandleForReading readDataToEndOfFile];
        [task waitUntilExit];
        NSString *output = [[NSString alloc] initWithData:outData encoding:NSUTF8StringEncoding] ?: @"";
        NSString *error = [[NSString alloc] initWithData:errData encoding:NSUTF8StringEncoding] ?: @"";
        dispatch_async(dispatch_get_main_queue(), ^{
            [self appendLog:error];
            [self appendLog:output];
            [self setBusy:NO message:task.terminationStatus == 0 ? @"操作完成" : @"操作未完成"];
            completion(task.terminationStatus, output, error);
        });
    });
}

- (void)refreshDevices:(id)sender {
    self.progress.doubleValue = 0;
    [self runTool:@[@"devices"] busy:@"正在查找 Keychron 鼠标…" completion:
        ^(int status, NSString *output, NSString *error) {
            NSDictionary *result = status == 0 ? [self parseResult:output] : nil;
            NSArray *devices = [result[@"devices"] isKindOfClass:NSArray.class] ? result[@"devices"] : @[];
            self.devices = devices;
            [self.devicePopup removeAllItems];
            if (!devices.count) {
                [self.devicePopup addItemWithTitle:@"未找到有线升级接口"];
                self.deviceLabel.stringValue = @"设备：未检测到可选择的 Keychron 鼠标";
                self.statusLabel.stringValue = @"请切换到有线模式、USB 直连后再刷新。";
                [self updateControls];
                return;
            }
            NSInteger firstCompatible = -1;
            for (NSUInteger index = 0; index < devices.count; index++) {
                NSDictionary *device = devices[index];
                NSString *name = device[@"display_name"] ?: device[@"product"] ?: @"Keychron Mouse";
                NSString *model = device[@"model"] ?: @"未知型号";
                NSString *version = device[@"firmware_version"] ?: @"未知版本";
                NSString *suffix = [device[@"compatible"] boolValue] ? @"" : @" · 不兼容";
                [self.devicePopup addItemWithTitle:[NSString stringWithFormat:@"%@ · %@ · %@%@",
                    name, model, version, suffix]];
                self.devicePopup.lastItem.representedObject = device;
                if (firstCompatible < 0 && [device[@"compatible"] boolValue]) firstCompatible = index;
            }
            if (firstCompatible >= 0) [self.devicePopup selectItemAtIndex:firstCompatible];
            [self deviceSelectionChanged:nil];
        }];
}

- (void)deviceSelectionChanged:(id)sender {
    NSDictionary *device = [self selectedDevice];
    if (!device) {
        [self updateControls];
        return;
    }
    self.deviceLabel.stringValue = [NSString stringWithFormat:
        @"设备：%@ · 型号 %@ · 当前固件 %@ · %@",
        device[@"display_name"] ?: @"Keychron Mouse", device[@"model"] ?: @"未知",
        device[@"firmware_version"] ?: @"未知", [self compatibilityLabelForDevice:device]];
    NSArray<NSString *> *warnings = [self warningsForDevice:device];
    if (![device[@"compatible"] boolValue]) {
        self.statusLabel.stringValue = [NSString stringWithFormat:@"设备不可升级：%@",
            [device[@"compatibility_reasons"] componentsJoinedByString:@"；"] ?: @"未知原因"];
    } else if (warnings.count) {
        self.statusLabel.stringValue = [NSString stringWithFormat:
            @"协议检查通过，但有需要确认的提示：%@",
            [warnings componentsJoinedByString:@"；"]];
    } else {
        self.statusLabel.stringValue = @"设备协议检查通过。请选择该型号对应的签名固件。";
    }
    [self updateControls];
}

- (void)probeDevice:(id)sender {
    NSString *deviceID = [self selectedDevice][@"device_id"];
    if (!deviceID.length) return;
    [self runTool:@[@"probe", @"--device", deviceID] busy:@"正在读取所选设备…" completion:
        ^(int status, NSString *output, NSString *error) {
            NSDictionary *result = status == 0 ? [self parseResult:output] : nil;
            if (!result) {
                [self showError:@"设备读取失败" details:error];
                return;
            }
            self.deviceLabel.stringValue = [NSString stringWithFormat:
                @"设备：%@ · 型号 %@ · 当前固件 %@ · %@",
                result[@"display_name"] ?: @"Keychron Mouse", result[@"model"] ?: @"未知",
                result[@"firmware_version"] ?: @"未知", [self compatibilityLabelForDevice:result]];
            NSArray<NSString *> *warnings = [self warningsForDevice:result];
            self.statusLabel.stringValue = warnings.count
                ? [NSString stringWithFormat:@"所选设备读取成功，但请注意：%@",
                    [warnings componentsJoinedByString:@"；"]]
                : @"所选设备读取成功。";
        }];
}

- (void)chooseFirmware:(id)sender {
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.title = @"选择与所选 Keychron 鼠标匹配的签名固件";
    panel.allowedContentTypes = @[[UTType typeWithFilenameExtension:@"bin"]];
    panel.allowsMultipleSelection = NO;
    if ([panel runModal] != NSModalResponseOK) return;
    self.firmwareURL = panel.URL;
    [self refreshFirmwareLabel:nil];
    [self updateControls];
    [self runTool:@[@"inspect", self.firmwareURL.path] busy:@"正在检查固件结构…" completion:
        ^(int status, NSString *output, NSString *error) {
            NSDictionary *result = status == 0 ? [self parseResult:output] : nil;
            if (!result) {
                self.firmwareURL = nil;
                [self refreshFirmwareLabel:nil];
                [self updateControls];
                [self showError:@"固件结构检查未通过" details:error];
                return;
            }
            [self refreshFirmwareLabel:result];
            self.statusLabel.stringValue = @"固件结构检查通过；开始升级前还会校验设备型号。";
        }];
}

- (void)beginUpgrade:(id)sender {
    NSDictionary *selected = [self selectedDevice];
    NSString *deviceID = selected[@"device_id"];
    if (!deviceID.length || !self.firmwareURL) {
        [self showError:@"信息不完整" details:@"请先选择鼠标和固件。"];
        return;
    }
    self.progress.doubleValue = 0;
    NSArray *checkArgs = @[@"upgrade", self.firmwareURL.path, @"--device", deviceID, @"--dry-run"];
    [self runTool:checkArgs busy:@"正在校验设备与固件兼容性…" completion:
        ^(int status, NSString *output, NSString *error) {
            NSDictionary *result = status == 0 ? [self parseResult:output] : nil;
            if (!result) {
                [self showError:@"设备与固件不匹配" details:error];
                return;
            }
            NSString *target = result[@"target_version"] ?: @"未知";
            if ([result[@"status"] isEqual:@"already_current"]) {
                self.progress.doubleValue = 100;
                self.statusLabel.stringValue = [NSString stringWithFormat:@"设备已经是目标版本 %@。", target];
                return;
            }
            NSDictionary *device = result[@"device"];
            NSString *model = device[@"model"] ?: @"";
            BOOL downgrade = [result[@"requires_downgrade_confirmation"] boolValue];
            BOOL unverifiedModel = [device[@"compatibility_status"] isEqual:@"protocol_compatible_unverified_model"];
            BOOL unverifiedProductID = [device[@"compatibility_status"] isEqual:@"protocol_compatible_unverified_product_id"];
            BOOL bootloaderOnly = [result[@"firmware_trust_status"] isEqual:@"bootloader_signature_only"];
            NSMutableArray<NSString *> *warnings = [NSMutableArray arrayWithObject:
                @"升级期间请保持 USB 连接。应用会阻止 Mac 自动休眠。"];
            if (unverifiedModel) [warnings addObject:@"该型号协议兼容，但尚未完成本项目实机升级验收。"];
            if (unverifiedProductID) [warnings addObject:@"该型号协议兼容，但此 USB PID 尚未完成本项目实机升级验收。"];
            if (bootloaderOnly) [warnings addObject:@"此固件不在可信哈希表中，签名真实性由设备 Bootloader 最终判断。"];
            if (downgrade) [warnings addObject:@"目标版本低于当前版本，这是降级操作。"];
            [warnings addObjectsFromArray:[self warningsForDevice:device]];

            NSAlert *alert = [[NSAlert alloc] init];
            alert.alertStyle = downgrade ? NSAlertStyleCritical : NSAlertStyleWarning;
            alert.messageText = [NSString stringWithFormat:@"确认将 %@ 升级到 %@？", model, target];
            alert.informativeText = [warnings componentsJoinedByString:@"\n"];
            [alert addButtonWithTitle:downgrade ? @"确认降级" : @"开始升级"];
            [alert addButtonWithTitle:@"取消"];
            if ([alert runModal] != NSAlertFirstButtonReturn) {
                self.statusLabel.stringValue = @"已取消，没有写入固件。";
                return;
            }

            NSMutableArray<NSString *> *args = [NSMutableArray arrayWithArray:
                @[@"upgrade", self.firmwareURL.path, @"--device", deviceID, @"--confirm", model]];
            if (downgrade) [args addObject:@"--allow-downgrade"];
            [self runTool:args busy:@"正在升级，请勿拔线…" completion:
                ^(int finalStatus, NSString *finalOutput, NSString *finalError) {
                    NSDictionary *finalResult = finalStatus == 0 ? [self parseResult:finalOutput] : nil;
                    if (!finalResult) {
                        [self showError:@"升级未完成" details:finalError];
                        return;
                    }
                    if ([finalResult[@"verified_after_restart"] boolValue]) {
                        self.progress.doubleValue = 100;
                        self.statusLabel.stringValue = [NSString stringWithFormat:
                            @"升级成功，重启后已验证版本 %@。", finalResult[@"target_version"] ?: target];
                        [self refreshDevices:nil];
                    } else {
                        [self showError:@"版本验证未通过" details:@"请查看日志并重新连接设备。"];
                    }
                }];
        }];
}

@end

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        NSApplication *app = NSApplication.sharedApplication;
        AppDelegate *delegate = [[AppDelegate alloc] init];
        app.delegate = delegate;
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];
        [app run];
    }
    return 0;
}
