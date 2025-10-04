import SwiftUI

struct SystemExtensionsView: View {
  @ObservedObject var systemExtensions = SystemExtensions.shared

  var body: some View {
    VStack(alignment: .leading, spacing: 0.0) {
      VStack(alignment: .leading, spacing: 12.0) {
        HStack(alignment: .center, spacing: 12.0) {
          Button(
            action: {
              let pboard = NSPasteboard.general
              pboard.clearContents()
              pboard.writeObjects([systemExtensions.list as NSString])
            },
            label: {
              Label("Copy to pasteboard", systemImage: "arrow.right.doc.on.clipboard")
            })

          Button(
            action: {
              SystemExtensions.shared.updateList()
            },
            label: {
              Label("Refresh", systemImage: "arrow.clockwise.circle")
            })
        }
      }
      .padding()
      .frame(maxWidth: .infinity, alignment: .leading)

      ScrollView {
        Text(systemExtensions.list)
          .lineLimit(nil)
          .textSelection(.enabled)
          .font(.callout)
          .monospaced()
          .padding()
          .frame(maxWidth: .infinity, alignment: .leading)
          .background(Color(NSColor.textBackgroundColor))
      }
      .border(Color(NSColor.separatorColor), width: 2)
    }
    .onAppear {
      SystemExtensions.shared.updateList()
    }
  }
}
