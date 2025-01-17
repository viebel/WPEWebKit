/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "WebStorageManagerInternal.h"

#import "StorageTracker.h"
#import "WebSecurityOriginInternal.h"
#import "WebStorageNamespaceProvider.h"
#import "WebStorageTrackerClient.h"
#import <WebCore/PageGroup.h>
#import <WebCore/SecurityOrigin.h>
#import <pthread.h>

using namespace WebCore;

NSString * const WebStorageDirectoryDefaultsKey = @"WebKitLocalStorageDatabasePathPreferenceKey";
NSString * const WebStorageDidModifyOriginNotification = @"WebStorageDidModifyOriginNotification";

static NSString *sLocalStoragePath;
static void initializeLocalStoragePath();
static pthread_once_t registerLocalStoragePath = PTHREAD_ONCE_INIT;

@implementation WebStorageManager

+ (WebStorageManager *)sharedWebStorageManager
{
    static WebStorageManager *sharedManager = [[WebStorageManager alloc] init];
    return sharedManager;
}

#if PLATFORM(IOS)
- (id)init
{
    if (!(self = [super init]))
        return nil;
    
    WebKitInitializeStorageIfNecessary();
    
    return self;
}
#endif

- (NSArray *)origins
{
    Vector<RefPtr<SecurityOrigin>> coreOrigins;

    WebKit::StorageTracker::tracker().origins(coreOrigins);

    NSMutableArray *webOrigins = [[NSMutableArray alloc] initWithCapacity:coreOrigins.size()];

    for (size_t i = 0; i < coreOrigins.size(); ++i) {
        WebSecurityOrigin *webOrigin = [[WebSecurityOrigin alloc] _initWithWebCoreSecurityOrigin:coreOrigins[i].get()];
        [webOrigins addObject:webOrigin];
        [webOrigin release];
    }

    return [webOrigins autorelease];
}

- (void)deleteAllOrigins
{
    WebKit::StorageTracker::tracker().deleteAllOrigins();
#if PLATFORM(IOS)
    // FIXME: This needs to be removed once StorageTrackers in multiple processes
    // are in sync: <rdar://problem/9567500> Remove Website Data pane is not kept in sync with Safari
    [[NSFileManager defaultManager] removeItemAtPath:[WebStorageManager _storageDirectoryPath] error:NULL];
#endif
}

- (void)deleteOrigin:(WebSecurityOrigin *)origin
{
    WebKit::StorageTracker::tracker().deleteOrigin([origin _core]);
}

- (unsigned long long)diskUsageForOrigin:(WebSecurityOrigin *)origin
{
    return WebKit::StorageTracker::tracker().diskUsageForOrigin([origin _core]);
}

- (void)syncLocalStorage
{
    WebKit::WebStorageNamespaceProvider::syncLocalStorage();
}

- (void)syncFileSystemAndTrackerDatabase
{
    WebKit::StorageTracker::tracker().syncFileSystemAndTrackerDatabase();
}

+ (NSString *)_storageDirectoryPath
{
    pthread_once(&registerLocalStoragePath, initializeLocalStoragePath);
    return sLocalStoragePath;
}

+ (void)setStorageDatabaseIdleInterval:(double)interval
{
    WebKit::StorageTracker::tracker().setStorageDatabaseIdleInterval(interval);
}

+ (void)closeIdleLocalStorageDatabases
{
    WebKit::WebStorageNamespaceProvider::closeIdleLocalStorageDatabases();
}

static void initializeLocalStoragePath()
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    sLocalStoragePath = [defaults objectForKey:WebStorageDirectoryDefaultsKey];
    if (!sLocalStoragePath || ![sLocalStoragePath isKindOfClass:[NSString class]]) {
        NSArray *paths = NSSearchPathForDirectoriesInDomains(NSLibraryDirectory, NSUserDomainMask, YES);
        NSString *libraryDirectory = [paths objectAtIndex:0];
        sLocalStoragePath = [libraryDirectory stringByAppendingPathComponent:@"WebKit/LocalStorage"];
    }
    sLocalStoragePath = [[sLocalStoragePath stringByStandardizingPath] retain];
}

void WebKitInitializeStorageIfNecessary()
{
    static BOOL initialized = NO;
    if (initialized)
        return;
    
    WebKit::StorageTracker::initializeTracker([WebStorageManager _storageDirectoryPath], WebStorageTrackerClient::sharedWebStorageTrackerClient());
        
    initialized = YES;
}

@end
