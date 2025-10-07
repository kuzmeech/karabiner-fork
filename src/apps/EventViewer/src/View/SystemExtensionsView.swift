import SwiftUI

struct SystemExtensionsView: View {
  var body: some View {
    VStack(alignment: .leading, spacing: 0.0) {
      VStack(alignment: .leading, spacing: 12.0) {
        HStack(alignment: .center, spacing: 12.0) {
          Button(
            action: {
              let pboard = NSPasteboard.general
              pboard.clearContents()
              pboard.writeObjects([SystemExtensions.shared.stream.text as NSString])
            },
            label: {
              Label("Copy to pasteboard", systemImage: "arrow.right.doc.on.clipboard")
            })

          Button(
            action: {
              SystemExtensions.shared.update()
            },
            label: {
              Label("Refresh", systemImage: "arrow.clockwise.circle")
            })
        }
      }
      .padding()
      .frame(maxWidth: .infinity, alignment: .leading)

      RealtimeText(
        stream: SystemExtensions.shared.stream,
        font: NSFont.monospacedSystemFont(
          ofSize: NSFont.preferredFont(forTextStyle: .callout).pointSize,
          weight: .regular)
      )
      .frame(maxWidth: .infinity, alignment: .leading)
      .background(Color(NSColor.textBackgroundColor))
      .border(Color(NSColor.separatorColor), width: 2)
      .padding(.leading, 2)  // Prevent the header underline from disappearing in NavigationSplitView.
    }
    .onAppear {
      SystemExtensions.shared.update()
    }
  }
}
